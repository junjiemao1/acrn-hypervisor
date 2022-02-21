#!/usr/bin/env python3
#
# Copyright (C) 2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import argparse, logging
from functools import cache, partialmethod
from collections import defaultdict
import lxml.etree as etree
from scenario_transformer import ScenarioTransformer

class VirtualUartConnections:
    class VirtualUartEndpoint:
        # The BDF of PCI virtual UARTs starts from 00:10.0
        next_dev = defaultdict(lambda: 16)

        @classmethod
        def from_endpoint_definition(cls, element):
            # For v2.x style scenario XML, the name of the VM is a child of a `vm` node.
            vm_name = element.xpath("ancestor::vm/name/text()").pop()
            if "legacy" in element.tag:
                base = element.find("base").text
                io_port = \
                    "0x3F8" if base.endswith("COM1_BASE") else \
                    "0x2F8" if base.endswith("COM2_BASE") else \
                    "0x3E8" if base.endswith("COM3_BASE") else \
                    "0x2E8" if base.endswith("COM4_BASE") else \
                    base
                return cls(vm_name, io_port = io_port)
            else:
                dev = cls.next_dev[vm_name]
                cls.next_dev[vm_name] += 1
                return cls(vm_name, pci_bdf = f"00:{dev:02x}.0")

        def __init__(self, vm_name, io_port = None, pci_bdf = None):
            self.vm_name = vm_name
            self.io_port = io_port
            self.pci_bdf = None

    class VirtualUartConnection:
        next_id = 1

        @classmethod
        def from_connection_definition(cls, element):
            name = element.get("name")
            ty = element.get("type")
            conn = cls(name = name, ty = ty)
            for endpoint in conn.findall("endpoint"):
                conn.add_endpoint(VirtualUartConnections.VirtualUartEndpoint.from_endpoint_definition(endpoint))
            return conn

        def __init__(self, name = None, ty = "legacy"):
            if name:
                self.name = name
            else:
                self.name = f"vUART connection {self.next_id}"
                self.__class__.next_id += 1
            self.ty = ty
            self.endpoints = []

        def add_endpoint(self, endpoint):
            self.endpoints.append(endpoint)

    def __init__(self):
        self.conns = []            # List of connections
        self.dangling_conns = {}   # (vm_id, vuart_id) -> conn whose target is the key

    def add_endpoint(self, element):
        """Parse the vUART endpoint definition in ACRN v2.x. Returns True if and only if the element is parsed properly."""

        try:
            key = (element.xpath("ancestor::vm/@id").pop(), element.xpath("@id").pop())
            if key in self.dangling_conns.keys():
                conn = self.dangling_conns.pop(key)
                conn.add_endpoint(self.VirtualUartEndpoint.from_endpoint_definition(element))
                self.conns.append(conn)
            else:
                ty = "legacy" if "legacy" in element.tag else "pci"
                conn = self.VirtualUartConnection(ty = ty)
                conn.add_endpoint(self.VirtualUartEndpoint.from_endpoint_definition(element))
                self.dangling_conns[(element.xpath("target_vm_id/text()").pop(), element.xpath("target_uart_id/text()").pop())] = conn

            return True
        except Exception as e:
            # Skip vUART endpoint definition not satisfying the schema. The discarded-data warnings will report those
            # unmigrated data.
            logging.debug(e)
            return False

    def add_connection(self, element):
        """Parse the vUART connection definition in ACRN v3.x"""
        self.conns.append(self.VirtualUartConnection.from_connection_definition(element))
        return True

    def format_xml_elements(self, xsd_element_node):
        new_nodes = []
        for conn in self.conns:
            new_node = etree.Element(xsd_element_node.get("name"))
            etree.SubElement(new_node, "name").text = conn.name
            etree.SubElement(new_node, "type").text = conn.ty
            for endpoint in conn.endpoints:
                new_endpoint_node = etree.SubElement(new_node, "endpoint")
                etree.SubElement(new_endpoint_node, "vm_name").text = endpoint.vm_name
                etree.SubElement(new_endpoint_node, "io_port").text = endpoint.io_port
            new_nodes.append(new_node)
        return new_nodes

class ScenarioUpgrader(ScenarioTransformer):
    def __init__(self, xsd_etree, old_xml_etree):
        super().__init__(xsd_etree, visit_optional_node=True)
        self.old_xml_etree = old_xml_etree

        # Collect all nodes in old_xml_etree which will be used to track data not moved
        self.old_data_nodes = set()
        for node in old_xml_etree.iter():
            if node.text:
                self.old_data_nodes.add(node)

        self.hv_vm_node_map = {}

    def get_from_old_data(self, new_parent_node, xpath):
        hv_vm_node = next(new_parent_node.iterancestors(["vm", "hv"]), None)
        old_hv_vm_node = self.hv_vm_node_map[hv_vm_node]
        old_data_node = old_hv_vm_node.xpath(xpath)
        return old_data_node

    def move_build_type(self, xsd_element_node, xml_parent_node, new_nodes):
        old_data_node = self.get_node(self.old_xml_etree, f"//hv//RELEASE")
        new_node = etree.Element(xsd_element_node.get("name"))

        if old_data_node is not None and old_data_node.text == "y":
            new_node.text = "release"
        else:
            new_node.text = "debug"

        new_nodes.append(new_node)
        self.old_data_nodes.discard(old_data_node)
        return False

    def move_loglevel(self, xsd_element_node, xml_parent_node, new_nodes):
        element_tag = xsd_element_node.get("name")
        old_data_node = super().get_node(self.old_xml_etree, f"//hv//{element_tag}")
        type_node = self.simple_type_of_element(xsd_element_node)

        if old_data_node is not None and type_node is not None:
            target_enum = super().get_node(type_node, f".//xs:enumeration[starts-with(@value, '{old_data_node.text[0]}')]/@value")
            if target_enum is None:
                target_enum = super().get_node(type_node, f".//xs:enumeration[last()]/@value")
                if target_enum is None:
                    target_enum = old_data_node.text

            new_node = etree.Element(element_tag)
            new_node.text = target_enum
            new_nodes.append(new_node)
            self.old_data_nodes.discard(old_data_node)

        return False

    def move_vuart_connections(self, xsd_element_node, xml_parent_node, new_nodes):
        conns = VirtualUartConnections()

        # Fetch vUART endpoints in the old data
        vuart_endpoints = self.old_xml_etree.xpath("//legacy_vuart[@id != '0' and base != 'INVALID_COM_BASE'] | //communication_vuart[base != 'INVALID_PCI_BASE']")
        vuart_connections = self.old_xml_etree.xpath("//vuart_connection")

        for endpoint in vuart_endpoints:
            if conns.add_endpoint(endpoint):
                for child in endpoint.iter():
                    self.old_data_nodes.discard(child)

        new_nodes.extend(conns.format_xml_elements(xsd_element_node))

        # Disconnected endpoints do not migrate, but remove such nodes from old_data_nodes to avoid raising
        # data-is-discarded warnings.
        for n in self.old_xml_etree.xpath("//legacy_vuart[@id != '0' and base = 'INVALID_COM_BASE'] | //communication_vuart[base = 'INVALID_PCI_BASE']"):
            for child in n.iter():
                self.old_data_nodes.discard(child)

        return False

    def move_ivshmem(self, xsd_element_node, xml_parent_node, new_nodes):
        return False

    def move_vm_type(self, xsd_element_node, xml_parent_node, new_nodes):
        old_vm_type_node = self.get_from_old_data(xml_parent_node, ".//vm_type").pop()
        old_guest_flag_nodes = self.get_from_old_data(xml_parent_node, ".//guest_flag[text() = 'GUEST_FLAG_RT']")

        new_node = etree.Element(xsd_element_node.get("name"))
        if old_vm_type_node.text in ["PRE_RT_VM", "POST_RT_VM"] or old_guest_flag_nodes:
            new_node.text = "RTVM"
        else:
            new_node.text = "STANDARD_VM"
        new_nodes.append(new_node)
        self.old_data_nodes.discard(old_vm_type_node)
        for n in old_guest_flag_nodes:
            self.old_data_nodes.discard(n)

        return False

    def move_guest_flag(self, guest_flag, xsd_element_node, xml_parent_node, new_nodes):
        new_node = etree.Element(xsd_element_node.get("name"))
        old_data_nodes = self.get_from_old_data(xml_parent_node, f".//guest_flag[text() = '{guest_flag}']")
        new_node.text = "y" if len(old_data_nodes) > 0 else "n"
        new_nodes.append(new_node)
        for n in old_data_nodes:
            self.old_data_nodes.discard(n)
        return False

    def move_data_by_xpath(self, xpath, xsd_element_node, xml_parent_node, new_nodes):
        element_tag = xsd_element_node.get("name")

        if self.complex_type_of_element(xsd_element_node) is None:
            old_data_nodes = self.get_from_old_data(xml_parent_node, xpath)
            max_occurs_raw = xsd_element_node.get("maxOccurs")

            # Use `len(old_data_nodes)` to ensure that all old data nodes are moved if an unbound number of
            # occurrences is allowed.
            max_occurs = \
                len(old_data_nodes)  if max_occurs_raw == "unbounded" else \
                1                    if max_occurs_raw is None        else \
                int(max_occurs_raw)

            if len(old_data_nodes) <= max_occurs:
                for n in old_data_nodes:
                    new_node = etree.Element(element_tag)
                    new_node.text = n.text
                    new_nodes.append(new_node)
                    self.old_data_nodes.discard(n)

            return False
        else:
            new_node = etree.Element(element_tag)
            new_nodes.append(new_node)
            return True

    def move_data_by_same_tag(self, xsd_element_node, xml_parent_node, new_nodes):
        element_tag = xsd_element_node.get("name")
        return self.move_data_by_xpath(f".//{element_tag}", xsd_element_node, xml_parent_node, new_nodes)

    def move_data_null(self, xsd_element_node, xml_parent_node, new_nodes):
        return False

    data_movers = {
        "basic/name": partialmethod(move_data_by_xpath, "./name"),
        "epc_section/base": partialmethod(move_data_by_xpath, ".//epc_section/base"),
        "epc_section/size": partialmethod(move_data_by_xpath, ".//epc_section/size"),

        "lapic_passthrough": partialmethod(move_guest_flag, "GUEST_FLAG_LAPIC_PASSTHROUGH"),
        "io_completion_polling": partialmethod(move_guest_flag, "GUEST_FLAG_IO_COMPLETION_POLLING"),
        "nested_virtualization_support": partialmethod(move_guest_flag, "GUEST_FLAG_NVMX_ENABLED"),
        "virtual_cat_support": partialmethod(move_guest_flag, "GUEST_FLAG_VCAT_ENABLED"),
        "secure_world_support": partialmethod(move_guest_flag, "GUEST_FLAG_SECURITY_VM"),
        "hide_mtrr_support": partialmethod(move_guest_flag, "GUEST_FLAG_HIDE_MTRR"),
        "security_vm": partialmethod(move_guest_flag, "GUEST_FLAG_SECURITY_VM"),

        "BUILD_TYPE": move_build_type,
        "MEM_LOGLEVEL": move_loglevel,
        "NPK_LOGLEVEL": move_loglevel,
        "CONSOLE_LOGLEVEL": move_loglevel,
        "vuart_connection": move_vuart_connections,
        "IVSHMEM": move_ivshmem,
        "name": move_data_null,                # Only VM names are migrated
        "vm_type": move_vm_type,
        "base": move_data_null,                # Only EPC section base addresses are migrated
        "default": move_data_by_same_tag,
    }

    def add_missing_nodes(self, xsd_element_node, xml_parent_node, xml_anchor_node):
        new_nodes = []
        def call_mover(mover):
            if isinstance(mover, partialmethod):
                return mover.__get__(self, type(self))(xsd_element_node, xml_parent_node, new_nodes)
            else:
                return mover(self, xsd_element_node, xml_parent_node, new_nodes)

        # Common names (such as 'name' or 'base') may be used as tags in multiple places each of which has different
        # meanings. In such cases it is ambiguious to query old data by that common tag alone.
        element_tag = xsd_element_node.get("name")
        element_tag_with_parent = f"{xml_parent_node.tag}/{element_tag}"

        mover_key = \
            element_tag_with_parent if element_tag_with_parent in self.data_movers.keys() else \
            element_tag if element_tag in self.data_movers.keys() else \
            "default"
        visit_children = call_mover(self.data_movers[mover_key])

        if xml_anchor_node is not None:
            for n in new_nodes:
                xml_anchor_node.addprevious(n)
        else:
            xml_parent_node.extend(new_nodes)

        if visit_children:
            return new_nodes
        else:
            return []

    @property
    @cache
    def upgraded_etree(self):
        new_xml_etree = etree.ElementTree(etree.Element(self.old_xml_etree.getroot().tag))
        root_node = new_xml_etree.getroot()
        for k, v in self.old_xml_etree.getroot().items():
            new_xml_etree.getroot().set(k, v)

        # Migrate the HV and VM nodes, which are needed to kick off a thorough traversal of the existing scenario.
        for old_node in self.old_xml_etree.getroot():
            new_node = etree.Element(old_node.tag)

            if old_node.tag == "vm":
                # FIXME: Here we still hard code how the load order of a VM is specified in different versions of
                # schemas. While it is not subject to frequent changes, it would be better if we use a more generic
                # approach instead.
                load_order_node = etree.SubElement(etree.SubElement(new_node, "hidden"), "load_order")

                # In the history we have two ways of specifying the load order of a VM: either by vm_type or by
                # loader_order.
                vm_type = old_node.xpath(".//vm_type/text()")
                vm_load_order = old_node.xpath(".//load_order/text()")
                if vm_type:
                    if vm_type[0].startswith("PRE_") or vm_type[0] in ["SAFETY_VM"]:
                        load_order_node.text = "PRE_LAUNCHED_VM"
                    elif vm_type[0].startswith("POST_"):
                        load_order_node.text = "POST_LAUNCHED_VM"
                    else:
                        load_order_node.text = "SERVICE_VM"
                elif vm_load_order:
                    load_order_node.text = vm_load_order[0]
                else:
                    logging.error(f"Cannot infer the loader order of VM {self.old_xml_etree.getelementpath(old_node)}")
                    continue

            root_node.append(new_node)
            for k, v in old_node.items():
                new_node.set(k, v)
            self.hv_vm_node_map[new_node] = old_node

        # Now fill the rest of configuration items using the old data
        self.transform(new_xml_etree)

        return new_xml_etree

def main(xsd_file, xml_file, out_file):
    xsd_etree = etree.parse(xsd_file)
    xsd_etree.xinclude()
    xml_etree = etree.parse(xml_file, etree.XMLParser(remove_blank_text=True))
    upgrader = ScenarioUpgrader(xsd_etree, xml_etree)
    upgrader.upgraded_etree.write(out_file, pretty_print=True)

    discarded_data = [(xml_etree.getelementpath(n), n.text) for n in upgrader.old_data_nodes]
    for path, data in sorted(discarded_data):
        logging.warning(f"{path} = {data} is discarded")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Try adapting data in a scenario XML to the latest schema.")
    parser.add_argument("xsd", help="Path to the schema of scenario XMLs")
    parser.add_argument("xml", help="Path to the scenario XML file from users")
    parser.add_argument("out", nargs="?", default="out.xml", help="Path where the output is placed")
    args = parser.parse_args()

    logging.basicConfig(level="DEBUG")
    main(args.xsd, args.xml, args.out)
