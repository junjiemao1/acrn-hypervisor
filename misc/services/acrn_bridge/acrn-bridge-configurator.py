#!/usr/bin/env python3
#
# Copyright (C) 2021-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

"""Create the network bridge 'acrn-br0' for running VMs on ACRN.

This script takes a positional argument which is interpreted as the command to
execute.

When invoked, this script creates a network bridge and connects a managed,
connected interface of a popular network manager, either systemd-networkd or
NetworkManager, to it. IP address configurations of the selected interface will
be moved to the created bridge. If multiple interfaces are available, users will
be prompt to specify one.

After creating a bridge, this script prompts users to confirm if the network
still works well, and allows users to roll back all the changes this script has
done if those changes break host network.

"""

import sys, os
import subprocess
import logging, argparse
import time

from itertools import chain
from configparser import ConfigParser
from uuid import uuid4
from copy import deepcopy
import yaml


def detect_util(name):
    """Detect the full path of a system utility program."""
    system_paths = [
            "/usr/bin/",
            "/usr/sbin/",
            "/usr/local/bin/",
    ]

    try:
        result = subprocess.run(["which", "-a", name], capture_output=True)
        assert result.returncode == 0
        for path in result.stdout.decode().strip().split("\n"):
            if any(map(lambda x: path.startswith(x), system_paths)):
                logging.debug(f"Use {name} found at {path}.")
                return path
    except:
        pass

    raise FileNotFoundError(f"Command '{name}' not found under common program directories.")

def ask_for_confirmation(message):
    try:
        while True:
            print(message, end=" ")
            sys.stdout.flush()
            response = sys.stdin.readline().strip()
            if response.lower() in ["y", "yes", "1", "t", "true"]:
                return True
            if response.lower() in ["n", "no", "0", "f", "false"]:
                return False
            print("Please answer Y or N.")
    except KeyboardInterrupt:
        return False

def ask_for_choice(message, choices, choice_description):
    try:
        while True:
            print(message, end=" ")
            sys.stdout.flush()
            response = sys.stdin.readline().strip()
            if response in choices:
                return response
            print(f"Please select a value among {choice_description}.")
    except KeyboardInterrupt:
        return None


class FileOperations:
    def __init__(self):
        self.__history = [];

    def __to_backup_path(self, path):
        return f"{path}.backup_by_acrn"

    def open_for_read(self, path):
        return open(path, "r")

    def backup(self, path):
        backup_path = self.__to_backup_path(path)
        if not os.path.exists(backup_path):
            os.rename(path, backup_path)
            logging.info(f"Renamed {path} -> {backup_path}")
            self.__history.append(("backup", path))
        else:
            raise FileExistsError(f"{backup_path} already exists and cannot be overwritten.")

    def create_for_write(self, path, mode=0o644):
        if not os.path.exists(path):
            self.__history.append(("create", path))
            logging.info(f"Created {path}")
            f = open(path, "w")
            os.chmod(path, mode)
            return f
        raise FileExistsError(f"{path} already exists and cannot be opened for write.")

    def rollback(self):
        for op, path in reversed(self.__history):
            try:
                if op == "backup":
                    os.rename(self.__to_backup_path(path), path)
                    logging.info(f"Renamed {self.__to_backup_path(path)} -> {path}")
                elif op == "create":
                    os.remove(path)
                    logging.info(f"Removed {path}")
            except (FileNotFoundError, PermissionError):
                pass

class NetworkInterface:
    def __init__(self, name):
        self.name = name
        self.manager = None
        self.config_file = None
        self.dev_path = None
        self.ip_address = []
        self.hw_address = None
        self.hw_address_fuller = None
        self.connection_id = None

    def short_description(self):
        text = f"{self.name}, managed by {self.manager.name}"
        if self.dev_path:
            text += f", Device Path: {self.dev_path}"
        if self.ip_address:
            text += f", IP Address: {self.ip_address}"
        if self.hw_address_fuller:
            text += f", MAC Address: {self.hw_address_fuller}"
        elif self.hw_address:
            text += f", MAC Address: {self.hw_address}"
        return text

    def __repr__(self):
        return f"<Network Interface '{self.name}' managed by {self.manager.name}>"

class NetworkBridge:
    def __init__(self, name):
        self.name = name
        self.manager = None
        self.config_file = None


class NetworkManagerConfig:
    def initialize_managed_links(self, managed_links):
        raise NotImplementedError

    def __init__(self):
        self.__managed_links = {}
        self.initialize_managed_links(self.__managed_links)

    def get_managed_interfaces(self):
        return list(filter(lambda x: isinstance(x, NetworkInterface), self.__managed_links.values()))

    def get_managed_bridge(self, name):
        link = self.__managed_links.get(name)
        if link is not None and isinstance(link, NetworkBridge):
            return link
        return None

    def remove_managed_interface(self, name):
        self.__managed_links.pop(name, None)

class NetworkdConfig(NetworkManagerConfig):
    name = "systemd-networkd"

    def initialize_managed_links(self, managed_links):
        # Enumerate the managed interfaces
        try:
            networkctl = detect_util("networkctl")
            result = subprocess.run([networkctl, "list", "--no-pager"], capture_output=True)
            assert result.returncode == 0 and result.stdout.decode(), \
                "Failed to get managed interface list from 'networkctl'."
            lines = result.stdout.decode().strip().split("\n")
            field_keys = [x for x in lines.pop(0).strip().split() if x]
        except (FileNotFoundError, AssertionError) as e:
            logging.error(f"{e} Interfaces configured by {self.name} will not be considered.")
            return

        # Skip the last two lines which are either empty or the number of links listed.
        for line in lines[:-2]:
            fields = dict(zip(field_keys, [x for x in line.split(" ") if x]))
            if fields["SETUP"] not in ["configured", "configuring"] or fields["OPERATIONAL"] != "routable":
                continue

            name = fields["LINK"]

            try:
                if fields["TYPE"] == "ether":
                    link = NetworkInterface(name)
                elif fields["TYPE"] == "bridge":
                    link = NetworkBridge(name)
                else:
                    continue

                result = subprocess.run([networkctl, "status", name, "-n", "0"], capture_output=True)
                assert result.returncode == 0 and result.stdout.decode(), \
                    f"Failed to get status of interface '{name}' from 'networkctl'."

                link.manager = self
                for line in result.stdout.decode().strip().split("\n"):
                    line = line.strip()
                    value = line.split(":", maxsplit=1)[-1].strip()
                    if line.startswith("Link File:"):
                        assert os.path.basename(value) == "99-default.link", \
                            "Link of interface '{name}' is specially configured."
                    elif line.startswith("Network File:"):
                        link.config_file = value
                        assert os.path.exists(value), \
                            f"Network file of interface '{name}' does not exist."
                    elif line.startswith("Path:"):
                        link.dev_path = value
                    elif line.startswith("Address:"):
                        link.ip_address = value
                    elif line.startswith("HW Address:"):
                        link.hw_address_fuller = value
                        link.hw_address = value.split(" ")[0]

                assert link.config_file != None, \
                    f"Failed to identify the network file for interface '{name}'."

                managed_links[name] = link
            except AssertionError as e:
                logging.error(f"{e} That interface will not be considered.")

    def reload_config(self):
        networkctl = detect_util("networkctl")
        result = subprocess.run([networkctl, "reload"])
        return result.returncode == 0

    def create_bridge(self, name, routable_interface, file_op):
        """Create a network bridge with the given routable interface.

        For systemd-networkd managed interfaces, connection configurations are written in INI format and creating a
        bridge requires the following steps:

          * Backup the network file of the routable interface given. In case that network file applies to multiple
            interfaces, a similar one will be generated with a different "Match" that explicitly matches the others
            after getting confirmation from users.

          * Create a netdev file declaring the bridge.

          * Create a network file setting network configurations (such as IP addresses) of the bridge.

          * Create a network file connecting the routable interface to the bridge.

        Return True if the creation succeeds. Otherewise return False.
        """

        try:
            iface_current_network_config = ConfigParser()
            iface_current_network_config.optionxform = str
            iface_current_network_config.read_file(file_op.open_for_read(routable_interface.config_file))

            iface_new_network_config = ConfigParser()
            iface_new_network_config.optionxform = str
            iface_new_network_config["Match"] = { "Name": routable_interface.name }
            iface_new_network_config["Network"] = { "Bridge": name }
            if "Link" in iface_current_network_config:
                iface_new_network_config["Link"] = iface_current_network_config["Link"]

            bridge_netdev_config = ConfigParser()
            bridge_netdev_config.optionxform = str
            bridge_netdev_config["NetDev"] = { "Name": name, "Kind": "bridge", "MACAddress": routable_interface.hw_address }

            bridge_network_config = ConfigParser()
            bridge_network_config.optionxform = str
            bridge_network_config["Match"] = { "Name": name }
            if "Network" in iface_current_network_config:
                bridge_network_config["Network"] = iface_current_network_config["Network"]
            if "Address" in iface_current_network_config:
                bridge_network_config["Address"] = iface_current_network_config["Address"]
            if "Route" in iface_current_network_config:
                bridge_network_config["Route"] = iface_current_network_config["Route"]
            if "DHCP" in iface_current_network_config:
                bridge_network_config["DHCP"] = iface_current_network_config["DHCP"]
            if "DHCPv4" in iface_current_network_config:
                bridge_network_config["DHCPv4"] = iface_current_network_config["DHCPv4"]

            # Check if there is any managed interface that is using the same network file.
            iface_sharing_config = list(filter(lambda x: x.config_file == routable_interface.config_file and x != routable_interface, \
                                               self.get_managed_interfaces()))
            if iface_sharing_config:
                logging.info(f"Network file {routable_interface.config_file} also manages the following network interfaces:")
                for iface in iface_sharing_config:
                    logging.info(f"\t{iface.short_description()}")
                if not ask_for_confirmation("Can we update that file to make it applicable only to the interfaces listed above? (Y/N)"):
                    return False

                iface_preserved_network_config = ConfigParser()
                iface_preserved_network_config.optionxform = str
                iface_preserved_network_config.read_file(file_op.open_for_read(routable_interface.config_file))
                iface_preserved_network_config["Match"] = { "Name": " ".join(map(lambda x: x.name, iface_sharing_config)) }
            else:
                iface_preserved_network_config = None

            config_dir = os.path.dirname(routable_interface.config_file)
            file_op.backup(routable_interface.config_file)
            if iface_preserved_network_config != None:
                iface_preserved_network_config.write(file_op.create_for_write(routable_interface.config_file))
            iface_new_network_config.write(file_op.create_for_write(os.path.join(config_dir, f"{routable_interface.name}.network")))
            bridge_netdev_config.write(file_op.create_for_write(os.path.join(config_dir, f"{name}.netdev")))
            bridge_network_config.write(file_op.create_for_write(os.path.join(config_dir, f"{name}.network")))

            self.reload_config()
        except FileExistsError as e:
            logging.error(f"{e}. Remove that file to continue.")
            logging.error(f"Applied file operations, if any, will be rolled back.")
            file_op.rollback()
            return False
        except PermissionError as e:
            logging.error(f"{e}. You may want to run this script as root or with root permission.")
            logging.error(f"Applied file operations, if any, will be rolled back.")
            file_op.rollback()
            return False

        return True

    def remove_bridge(self, name, routable_interface, file_op):
        file_op.rollback()
        self.reload_config()


class NetworkManagerConfig(NetworkManagerConfig):
    name = "NetworkManager"

    config_dirs = ["/etc/NetworkManager", "/lib/NetworkManager", "/run/NetworkManager"]

    def __link_id(self, name):
        return f"ACRN External Link of Bridge {name}"

    def __bridge_id(self, name):
        return f"ACRN Bridge {name}"

    def initialize_managed_links(self, managed_links):
        # Enumerate the managed interfaces
        try:
            nmcli = detect_util("nmcli")
            field_keys = ["DEVICE", "TYPE", "STATE", "FILENAME"]
            result = subprocess.run([nmcli, "-t", f"--fields={','.join(field_keys)}", "connection", "show", "--active"], capture_output=True)
            assert result.returncode == 0, "Failed to get managed interface list from 'nmcli'."
            assert result.stdout.decode(), "No NetworkManager managed connection is found."
            lines = result.stdout.decode().strip().split("\n")
        except (FileNotFoundError, AssertionError) as e:
            logging.error(f"{e} Interfaces configured by {self.name} will not be considered.")
            return

        for line in lines:
            fields = dict(zip(field_keys, [x for x in line.split(":") if x]))
            if fields["STATE"] != "activated":
                continue

            name = fields["DEVICE"]

            try:
                if fields["TYPE"].endswith("ethernet"):
                    link = NetworkInterface(name)
                elif fields["TYPE"] == "bridge":
                    link = NetworkBridge(name)
                else:
                    continue

                result = subprocess.run([nmcli, "-t", "device", "show", name], capture_output=True)
                assert result.returncode == 0 and result.stdout.decode(), \
                    f"Failed to get status of interface '{name}' from 'nmcli'."

                link.manager = self
                link.config_file = fields["FILENAME"]

                # Exclude any link whose config file is not under typical NetworkManager config directories. This is
                # mainly to exclude links that are managed by /etc/network/*
                assert any(map(lambda x: link.config_file.startswith(x), self.config_dirs)), \
                    f"Config file {link.config_file} is not under typical NetworkManager config directories."

                for line in result.stdout.decode().strip().split("\n"):
                    line = line.strip()
                    value = line.split(":", maxsplit=1)[-1].strip()
                    if line.startswith("IP4.ADDRESS"):
                        link.ip_address = value
                    elif line.startswith("GENERAL.HWADDR:"):
                        link.hw_address_fuller = value
                        link.hw_address = value

                managed_links[name] = link
            except AssertionError as e:
                logging.error(f"{e} That interface will not be considered.")

    def reload_config(self):
        nmcli = detect_util("nmcli")
        result = subprocess.run([nmcli, "connection", "reload"])
        return result.returncode == 0

    def change_connection_status(self, device, turn_on):
        nmcli = detect_util("nmcli")
        if turn_on:
            result = subprocess.run([nmcli, "connection", "up", device])
        else:
            result = subprocess.run([nmcli, "connection", "down", device])
        return result.returncode == 0

    def wait_for_ip_release(self, device, timeout=5):
        nmcli = detect_util("nmcli")
        count = 0
        logging.info(f"Waiting for IP address(es) to be released from {device}.")
        while count < timeout:
            result = subprocess.run([nmcli, "-t", "device", "show", device], capture_output=True)
            if result.returncode != 0 or not result.stdout.decode():
                raise ValueError
            fields = [x.split(":")[0] for x in result.stdout.decode().strip().split("\n")]
            if "IP4.ADDRESS" not in fields and "IP6.ADDRESS" not in fields:
                logging.info(f"IP addresses(es) released from {device}.")
                return
            time.sleep(1)

        raise TimeoutError

    def create_bridge(self, name, routable_interface, file_op):
        """Create a network bridge with the given routable interface.

        For NetworkManager managed interfaces, connection configurations are written in INI format and creating a
        bridge requires the following steps:

          * Backup the network file of the routable interface given.

          * Create a netdev file declaring the bridge.

          * Create a network file setting network configurations (such as IP addresses) of the bridge.

          * Create a network file connecting the routable interface to the bridge.

        Return True if the creation succeeds. Otherewise return False.
        """

        try:
            iface_current_config = ConfigParser()
            iface_current_config.read_file(file_op.open_for_read(routable_interface.config_file))
            routable_interface.connection_id = iface_current_config["connection"]["id"]

            bridge_config = ConfigParser()
            bridge_config["connection"] = {
                "id": self.__bridge_id(name),
                "uuid": str(uuid4()),
                "type": "bridge",
                "interface-name": name,
                "permissions": "",
            }
            bridge_config["bridge"] = { "mac-address": routable_interface.hw_address }
            if "ipv4" in iface_current_config:
                bridge_config["ipv4"] = iface_current_config["ipv4"]
            if "ipv6" in iface_current_config:
                bridge_config["ipv6"] = iface_current_config["ipv6"]
            if "proxy" in iface_current_config:
                bridge_config["proxy"] = iface_current_config["proxy"]

            iface_new_config = ConfigParser()
            iface_new_config["connection"] = {
                "id": self.__link_id(name),
                "uuid": str(uuid4()),
                "type": iface_current_config["connection"]["type"],
                "interface-name": iface_current_config["connection"]["interface-name"],
                "master": name,
                "permissions": "",
                "slave-type": "bridge",
            }
            iface_new_config["ethernet"] = {
                "mac-address": routable_interface.hw_address,
                "mac-address-blacklist": ""
            }
            iface_new_config["bridge-port"] = {}

            config_dir = os.path.dirname(routable_interface.config_file)
            file_op.backup(routable_interface.config_file)
            iface_new_config.write(file_op.create_for_write(os.path.join(config_dir, f"{iface_new_config['connection']['id']}.nmconnection"), mode=0o600))
            bridge_config.write(file_op.create_for_write(os.path.join(config_dir, f"{bridge_config['connection']['id']}.nmconnection"), mode=0o600))

            self.change_connection_status(routable_interface.connection_id, False)
            self.wait_for_ip_release(routable_interface.name)
            self.reload_config()
            self.change_connection_status(self.__bridge_id(name), True)
        except FileExistsError as e:
            logging.error(f"{e}. Remove that file to continue.")
            file_op.rollback()
            logging.error(f"Applied file operations, if any, are rolled back.")
            return False
        except PermissionError as e:
            logging.error(f"{e}. You may want to run this script as root or with root permission.")
            file_op.rollback()
            logging.error(f"Applied file operations, if any, are rolled back.")
            return False
        except (ValueError, TimeoutError):
            logging.error(f"IP address(es) are not released after {routable_interface.connection_id} is deactivated.")
            self.change_connection_status(routable_interface.connection_id, False)
            file_op.rollback()
            logging.error(f"Applied file and connection operations are rolled back.")
            return False

        return True

    def remove_bridge(self, name, routable_interface, file_op):
        self.change_connection_status(self.__bridge_id(name), False)
        file_op.rollback()
        self.reload_config()
        self.change_connection_status(routable_interface.connection_id, True)


class NetplanConfig(NetworkManagerConfig):
    name = "netplan"

    config_dirs = ["/etc/netplan", "/lib/netplan", "/run/netplan"]

    def initialize_managed_links(self, managed_links):
        for renderer in self.__renderers:
            for iface in renderer.get_managed_interfaces():
                if iface.config_file.startswith("/run"):
                    for path, yaml_content in self.__yaml_contents.items():
                        try:
                            yaml_content["network"]["ethernets"][iface.name]
                            iface.manager = self
                            iface.config_file = path
                            renderer.remove_managed_interface(iface.name)
                            managed_links[iface.name] = iface
                            break
                        except KeyError:
                            pass

    def reload_config(self):
        netplan = detect_util("netplan")
        result = subprocess.run([netplan, "apply"])
        return result.returncode == 0

    def create_bridge(self, name, routable_interface, file_op):
        """Create a network bridge with the given routable interface.

        For netplan managed interfaces, connection configurations are written in YAML format and creating a bridge
        requires the following steps:

          * Backup the yaml file of the routable interface given.

          * Create a yaml file of the same name with DHCP on the routable interface disabled.

          * Create another yaml file defining the bridge.

        Return True if the creation succeeds. Otherewise return False.

        """

        try:
            iface_current_config = self.__yaml_contents[routable_interface.config_file]

            bridge_config = {
                "network": {
                    "bridges": {
                        name: {
                            "macaddress": routable_interface.hw_address,
                            "interfaces": [routable_interface.name],
                        }
                    }
                }
            }
            for k,v in iface_current_config["network"]["ethernets"][routable_interface.name].items():
                if k not in ["match", "set-name"]:
                    bridge_config["network"]["bridges"][name][k] = v

            iface_new_config = deepcopy(iface_current_config)
            iface_new_config["network"]["ethernets"][routable_interface.name] = {
                "dhcp4": "no",
                "dhcp6": "no",
            }

            config_dir = os.path.dirname(routable_interface.config_file)
            file_op.backup(routable_interface.config_file)
            yaml.dump(iface_new_config, file_op.create_for_write(os.path.join(config_dir, routable_interface.config_file)))
            yaml.dump(bridge_config, file_op.create_for_write(os.path.join(config_dir, f"90-{name}.yaml")))

            self.reload_config()
        except FileExistsError as e:
            logging.error(f"{e}. Remove that file to continue.")
            file_op.rollback()
            logging.error(f"Applied file operations, if any, are rolled back.")
            return False
        except PermissionError as e:
            logging.error(f"{e}. You may want to run this script as root or with root permission.")
            file_op.rollback()
            logging.error(f"Applied file operations, if any, are rolled back.")
            return False

        return True

    def remove_bridge(self, name, routable_interface, file_op):
        file_op.rollback()
        self.reload_config()

    def __init__(self, renderers):
        self.__renderers = renderers
        self.__yaml_contents = {}

        for config_dir in self.config_dirs:
            try:
                for filename in os.listdir(config_dir):
                    path = os.path.join(config_dir, filename)
                    if path.endswith(".yaml") and os.path.isfile(path):
                        with open(path, "r") as fp:
                            self.__yaml_contents[path] = yaml.safe_load(fp)
            except FileNotFoundError:
                pass

        super().__init__()


def get_target_interface(network_managers):
    candidate_interfaces = list(chain.from_iterable(map(lambda x: x.get_managed_interfaces(), network_managers)))
    if len(candidate_interfaces) == 0:
        logging.error("No managed routable interface detected.")
        return None
    elif len(candidate_interfaces) == 1:
        logging.info("The following network interface will be connected to the bridge:")
        logging.info(f"\t{candidate_interfaces[0].short_description()}")
        logging.info("!!! Your IP address MAY CHANGE after creating the bridge !!!")
        if ask_for_confirmation("Is it ok? (Y/N)"):
            return candidate_interfaces[0]
    else:
        print("Multiple connected network interfaces detected:")
        for idx, iface in enumerate(candidate_interfaces, start=1):
            print(f"\t{idx}. {iface.short_description()}")
        nr_interfaces = len(candidate_interfaces)
        choice = ask_for_choice(f"Choose a network interface to connect to the new bridge (1-{nr_interfaces}):", \
                                list(map(str, range(1, nr_interfaces + 1))), f"integers 1-{nr_interfaces}")
        if choice != None:
            return candidate_interfaces[int(choice) - 1]

        return None

def create_bridge(network_managers, target_bridge_name):
    if any(map(lambda x: x.get_managed_bridge(target_bridge_name) != None, network_managers)):
        logging.info(f"'{target_bridge_name}' is already created and managed. Nothing to do.")
        sys.exit(0)

    target_interface = get_target_interface(network_managers)
    if target_interface == None:
        sys.exit(1)

    logging.info(f"Bridge '{target_bridge_name}' will have this link connected: {target_interface.short_description()}.")

    file_op = FileOperations()
    if target_interface.manager.create_bridge(target_bridge_name, target_interface, file_op):
        logging.info(f"Bridge '{target_bridge_name}' has been created successfully.")
        response = ask_for_confirmation(f"Does your network function well after the bridge is created? (If not, we can roll back the settings for you) (Y/N)")
        if not response:
            target_interface.manager.remove_bridge(target_bridge_name, target_interface, file_op)
            logging.info(f"Network configurations have been rolled back successfully.")
    else:
        logging.error(f"Failed to create bridge '{target_bridge_name}'.")
        sys.exit(1)

def main(command):
    command_handlers = {
        "create": create_bridge,
    }
    if command in command_handlers:
        target_bridge_name = "acrn-br0"

        networkd = NetworkdConfig()
        network_manager = NetworkManagerConfig()
        netplan = NetplanConfig([networkd, network_manager])
        network_managers = [
            networkd,
            network_manager,
            netplan,
        ]

        command_handlers[command](network_managers, target_bridge_name)
    else:
        logging.error(f"Unknown command '{command}'.")
        logging.error(f"The following commands are supported: {', '.join(command_handlers.keys())}.")
        sys.exit(1)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    args = parser.parse_args()

    logging.basicConfig(level="INFO", format="")
    main("create")
