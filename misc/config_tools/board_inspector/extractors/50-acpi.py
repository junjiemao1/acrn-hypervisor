# Copyright (C) 2021 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import logging
import lxml.etree
from collections import defaultdict

from acpiparser import parse_dsdt, parse_resource_data, parse_pci_routing
from acpiparser.aml.tree import Visitor, Direction
import acpiparser.aml.builder as builder
import acpiparser.aml.context as context
import acpiparser.aml.datatypes as datatypes
from acpiparser.aml.interpreter import ConcreteInterpreter
from acpiparser.aml.exception import UndefinedSymbol, FutureWork
from acpiparser.aml.visitors import GenerateBinaryVisitor
from acpiparser.rdt import *

from extractors.helpers import add_child, get_node

device_objects = defaultdict(lambda: {})                       # device_path -> object_name -> tree
device_deps = defaultdict(lambda: defaultdict(lambda: set()))     # device_path -> dep_type -> {device_path}
DEP_TYPE_USES = "uses"
DEP_TYPE_USED_BY = "is used by"
DEP_TYPE_CONSUMES = "consumes resources from"
DEP_TYPE_PROVIDES = "provides resources to"

def parse_eisa_id(eisa_id):
    chars = [
        (eisa_id & 0x7c) >> 2,                                # Bit 6:2 of the first byte
        ((eisa_id & 0x3) << 3) | ((eisa_id & 0xe000) >> 13),  # Bit 1:0 of the first byte and bit 7:5 of the second
        (eisa_id & 0x1F00) >> 8,                              # Bit 4:0 of the second byte
        (eisa_id & 0x00F00000) >> 20,                         # Bit 7:4 of the third byte
        (eisa_id & 0x000F0000) >> 16,                         # Bit 3:0 of the third byte
        (eisa_id & 0xF0000000) >> 28,                         # Bit 7:4 of the fourth byte
        (eisa_id & 0x0F000000) >> 24,                         # Bit 3:0 of the fourth byte
    ]
    if all(map(lambda x:x <= (ord('Z') - 0x40), chars[:3])):
        manufacturer = ''.join(map(lambda x: chr(x + 0x40), chars[:3]))
        product = ''.join(map(lambda x: "%X" % x, chars[3:6]))
        revision = "%X" % chars[6]
        return manufacturer + product + revision
    else:
        return None

predefined_nameseg = {
    "_SB_": ("bus", "system"),
    "_TZ_": ("thermalzone", None),
}

buses = {
    "PNP0A03": "pci",
    "PNP0A08": "pci",
}

def get_device_element(devices_node, namepath, hid):
    assert namepath.startswith("\\")
    namesegs = namepath[1:].split(".")

    element = devices_node
    for i,nameseg in enumerate(namesegs):
        buspath = f"\\{'.'.join(namesegs[:(i+1)])}"
        tag, typ = "device", None
        if nameseg in predefined_nameseg.keys():
            tag, typ = predefined_nameseg[nameseg]
        next_element = None
        for child in element:
            acpi_object = get_node(child, "acpi_object")
            if acpi_object is not None and acpi_object.text == buspath:
                next_element = child
                break
        if next_element is None:
            next_element = add_child(element, tag, None)
            add_child(next_element, "acpi_object", buspath)
            if typ:
                next_element.set("type", typ)
        element = next_element

    if hid:
        element.set("id", hid)
    return element

def parse_irq(idx, item, elem):
    irqs = ", ".join(map(str, item.irqs))
    add_child(elem, "resource", id=f"res{idx}", type="irq", int=irqs)

def parse_io_port(idx, item, elem):
    add_child(elem, "resource", id=f"res{idx}", type="io_port", min=hex(item._MIN), max=hex(item._MAX), len=hex(item._LEN))

def parse_fixed_io_port(idx, item, elem):
    add_child(elem, "resource", id=f"res{idx}", type="io_port", min=hex(item._BAS), max=hex(item._BAS + item._LEN - 1), len=hex(item._LEN))

def parse_fixed_memory_range(idx, item, elem):
    add_child(elem, "resource", id=f"res{idx}", type="memory", min=hex(item._BAS), max=hex(item._BAS + item._LEN - 1), len=hex(item._LEN))

def parse_address_space_resource(idx, item, elem):
    if item._TYP == 0:
        typ = "memory"
    elif item._TYP == 1:
        typ = "io_port"
    elif item._TYP == 2:
        typ = "bus_number"
    else:
        typ = "custom"
    add_child(elem, "resource", id=f"res{idx}", type=typ, min=hex(item._MIN), max=hex(item._MAX), len=hex(item._LEN))

def parse_extended_irq(idx, item, elem):
    irqs = ", ".join(map(str, item._INT))
    add_child(elem, "resource", id=f"res{idx}", type="irq", int=irqs)

resource_parsers = {
    (0, SMALL_RESOURCE_ITEM_IRQ_FORMAT): parse_irq,
    (0, SMALL_RESOURCE_ITEM_IO_PORT): parse_io_port,
    (0, SMALL_RESOURCE_ITEM_FIXED_LOCATION_IO_PORT): parse_fixed_io_port,
    (0, SMALL_RESOURCE_ITEM_END_TAG): (lambda x,y,z: None),
    (1, LARGE_RESOURCE_ITEM_32BIT_FIXED_MEMORY_RANGE): parse_fixed_memory_range,
    (1, LARGE_RESOURCE_ITEM_ADDRESS_SPACE_RESOURCE): parse_address_space_resource,
    (1, LARGE_RESOURCE_ITEM_WORD_ADDRESS_SPACE): parse_address_space_resource,
    (1, LARGE_RESOURCE_ITEM_EXTENDED_INTERRUPT): parse_extended_irq,
    (1, LARGE_RESOURCE_ITEM_QWORD_ADDRESS_SPACE): parse_address_space_resource,
    (1, LARGE_RESOURCE_ITEM_EXTENDED_ADDRESS_SPACE): parse_address_space_resource,
}

class CollectDependencyVisitor(Visitor):
    class AnalysisResult:
        def __init__(self):
            self.direct = defaultdict(lambda: list())  # device path -> [decls]
            self.all = defaultdict(lambda: list())  # device path -> [decls]

        def add_direct_dep(self, scope_decl, decl):
            scope_name = scope_decl.name if scope_decl else "global"
            if decl not in self.direct[scope_name]:
                self.direct[scope_name].append(decl)
            if decl not in self.all[scope_name]:
                self.all[scope_name].append(decl)

        def add_indirect_dep(self, scope_decl, decl):
            scope_name = scope_decl.name if scope_decl else "global"
            if decl not in self.all[scope_name]:
                self.all[scope_name].append(decl)

    def __init__(self, context):
        super().__init__(Direction.TOPDOWN)
        self.context = context

    def analyze(self, path):
        self.result = self.AnalysisResult()
        self.tree_under_analysis = self.context.lookup_symbol(path).tree
        self.to_visit = set([self.tree_under_analysis])
        self.current_tree = None
        visited = set()

        while self.to_visit:
            self.current_tree = self.to_visit.pop()
            if self.current_tree not in visited:
                self.visit(self.current_tree)
                visited.add(self.current_tree)

        return self.result

    def __add_dependency(self, decl):
        scope_decl = decl
        try:
            while not isinstance(scope_decl, context.DeviceDecl):
                scope_decl = self.context.lookup_symbol(self.context.parent(scope_decl.name))
                if scope_decl.tree == self.current_tree:
                    return
        except UndefinedSymbol:
            scope_decl = None

        if self.current_tree == self.tree_under_analysis:
            self.result.add_direct_dep(scope_decl, decl)
        else:
            self.result.add_indirect_dep(scope_decl, decl)

    def NameString(self, tree):
        self.context.change_scope(tree.scope)

        try:
            decl = self.context.lookup_symbol(tree.value)

            if isinstance(decl, context.OperationFieldDecl) and isinstance(decl.region, str):
                # Also visit the operation region in which the field is defined
                op_region_decl = self.context.lookup_symbol(decl.region)
                if isinstance(op_region_decl, context.OperationRegionDecl):
                    self.__add_dependency(op_region_decl)
                    self.to_visit.add(op_region_decl.tree)
            elif isinstance(decl, context.MethodDecl):
                self.to_visit.add(decl.tree)

            # Do not record the object under analysis as a dependency
            if decl.tree != self.tree_under_analysis:
                self.__add_dependency(decl)
        except UndefinedSymbol:
            pass

        self.context.pop_scope()

def add_object_to_device(interpreter, device_path, obj_name, result):
    def get_fresh_name(device_path):
        fmt = "unnamed_{}"
        for i in range(1, 100):
            candidate = fmt.format(i)
            if candidate not in device_objects[device_path].keys():
                return candidate
        raise NotImplementedError(f"{device_path}: Too many unnamed objects.")
    def aux(device_path, obj_name, result):
        if not obj_name in device_objects[device_path].keys():
            visitor = CollectDependencyVisitor(interpreter.context)
            deps = visitor.analyze(f"{device_path}.{obj_name}")
            copy_object = False

            if deps.all:
                # If the object refers to any operation region directly or indirectly, it is generally necessary to copy
                # the original definition of the object.
                for dev, decls in deps.all.items():
                    if dev != "global" and set(filter(lambda x: isinstance(x, context.OperationRegionDecl), decls)):
                        copy_object = True
                        break

            if result == None or copy_object:
                if "global" in deps.direct.keys():
                    global_objs = ', '.join(map(lambda x: x.name, deps.all["global"]))
                    raise NotImplementedError(f"{device_path}.{obj_name}: references to global objects: {global_objs}")

                # Add directly referred objects first
                for peer_device, peer_decls in deps.direct.items():
                    for peer_decl in peer_decls:
                        peer_obj_name = peer_decl.name[-4:]
                        if isinstance(peer_decl, context.OperationRegionDecl):
                            aux(peer_device, peer_obj_name, None)
                        elif isinstance(peer_decl, context.OperationFieldDecl):
                            device_objects[peer_device][get_fresh_name(peer_device)] = peer_decl.parent_tree
                        else:
                            if isinstance(peer_decl, context.MethodDecl) and peer_decl.nargs > 0:
                                raise NotImplementedError(f"{peer_decl.name}: copy of methods with arguments is not supported")
                            value = interpreter.interpret_method_call(peer_decl.name)
                            aux(peer_device, peer_obj_name, value)

                        # Declare decl as an external symbol in the template of device_path so that the template can be
                        # parsed on its own
                        device_objects[device_path][get_fresh_name(device_path)] = builder.DefExternal(
                            peer_decl.name,
                            peer_decl.object_type(),
                            peer_decl.nargs if isinstance(peer_decl, context.MethodDecl) else 0)
                        device_deps[device_path][DEP_TYPE_USES].add(peer_device)
                        device_deps[peer_device][DEP_TYPE_USED_BY].add(device_path)

                decl = interpreter.context.lookup_symbol(f"{device_path}.{obj_name}")
                device_objects[device_path][obj_name] = decl.tree
            else:
                tree = builder.build_value(result)
                if tree:
                    device_objects[device_path][obj_name] = builder.DefName(obj_name, tree)
                else:
                    raise NotImplementedError(f"{device_path}.{obj_name}: unrecognized type: {result.__class__.__name__}")

    try:
        aux(device_path, obj_name, result)

        # A device also depends on resource providers. If the given object is a resource template, scan for the encoded
        # resource sources.
        if obj_name == "_CRS":
            namespace = interpreter.context
            rdt = parse_resource_data(result.get())
            for item in rdt.items:
                source = getattr(item, "resource_source", None)
                if source:
                    source = source.decode("ascii")
                    namespace.change_scope(device_path)
                    try:
                        peer_device = namespace.lookup_symbol(namespace.normalize_namepath(source)).name
                        device_deps[device_path][DEP_TYPE_CONSUMES].add(peer_device)
                        device_deps[peer_device][DEP_TYPE_PROVIDES].add(device_path)
                    except:
                        pass
                    namespace.pop_scope()
    except NotImplementedError as e:
        logging.info(f"{device_path}.{obj_name}: will not be added to vACPI, reason: {str(e)}")

def fetch_device_info(devices_node, interpreter, namepath):
    logging.info(f"Fetch information about device object {namepath}")

    try:
        # Check if an _INI method exists
        try:
            interpreter.interpret_method_call(namepath + "._INI")
        except UndefinedSymbol:
            pass

        sta = None
        if interpreter.context.has_symbol(f"{namepath}._STA"):
            result = interpreter.interpret_method_call(f"{namepath}._STA")
            sta = result.get()
            if sta & 0x1 == 0:
                return
            add_object_to_device(interpreter, namepath, "_STA", result)

        # Hardware ID
        hid = ""
        if interpreter.context.has_symbol(f"{namepath}._HID"):
            result = interpreter.interpret_method_call(f"{namepath}._HID")
            hid = result.get()
            if isinstance(hid, str):
                pass
            elif isinstance(hid, int):
                eisa_id = parse_eisa_id(hid)
                if eisa_id:
                    hid = eisa_id
                else:
                    hid = hex(hid)
            else:
                hid = "<unknown>"
            add_object_to_device(interpreter, namepath, "_HID", result)

        # Compatible ID
        cids = []
        if interpreter.context.has_symbol(f"{namepath}._CID"):
            cid_object = interpreter.interpret_method_call(f"{namepath}._CID")
            if isinstance(cid_object, (datatypes.String, datatypes.Integer)):
                cid_data = [cid_object]
            elif isinstance(cid_object, datatypes.Package):
                cid_data = cid_object.elements

            for cid_datum in cid_data:
                if isinstance(cid_datum, datatypes.Integer):
                    eisa_id = parse_eisa_id(cid_datum.get())
                    if eisa_id:
                        cids.append(eisa_id)
                    else:
                        cids.append(hex(cid_datum.get()))
                elif isinstance(cid_datum, datatypes.String):
                    cids.append(cid_datum.get())

        # Create the XML element for the device and create its ancestors if necessary
        element = get_device_element(devices_node, namepath, hid)
        if hid in buses.keys():
            element.tag = "bus"
            element.set("type", buses[hid])
        for cid in cids:
            add_child(element, "compatible_id", cid)

        # Unique ID
        uid = ""
        if interpreter.context.has_symbol(f"{namepath}._UID"):
            result = interpreter.interpret_method_call(f"{namepath}._UID")
            uid = result.get()
            add_child(element, "acpi_uid", str(uid))
            add_object_to_device(interpreter, namepath, "_UID", result)

        # Description
        if interpreter.context.has_symbol(f"{namepath}._STR"):
            result = interpreter.interpret_method_call(f"{namepath}._STR")
            desc = result.get().decode(encoding="utf-16").strip("\00")
            element.set("description", desc)
            add_object_to_device(interpreter, namepath, "_STR", result)

        # Address
        if interpreter.context.has_symbol(f"{namepath}._ADR"):
            result = interpreter.interpret_method_call(f"{namepath}._ADR")
            adr = result.get()
            if isinstance(adr, int):
                adr = hex(adr)
            if len(element.xpath(f"../*[@address='{adr}']")) > 0:
                logging.warning(f"{namepath} has siblings with duplicated address {adr}.")
            else:
                element.set("address", hex(adr) if isinstance(adr, int) else adr)
            add_object_to_device(interpreter, namepath, "_ADR", result)

        # Status
        if sta is not None:
            status = add_child(element, "status")

            add_child(status, "present", "y" if sta & 0x1 != 0 else "n")
            add_child(status, "enabled", "y" if sta & 0x2 != 0 else "n")
            add_child(status, "functioning", "y" if sta & 0x8 != 0 else "n")

        # Resources
        if interpreter.context.has_symbol(f"{namepath}._CRS"):
            result = interpreter.interpret_method_call(f"{namepath}._CRS")
            data = result.get()
            rdt = parse_resource_data(data)

            for idx, item in enumerate(rdt.items):
                p = (item.type, item.name)
                if p in resource_parsers.keys():
                    resource_parsers[p](idx, item, element)
                else:
                    add_child(element, "resource", type=item.__class__.__name__, id=f"res{idx}")

            add_object_to_device(interpreter, namepath, "_CRS", result)

        # PCI interrupt routing
        if interpreter.context.has_symbol(f"{namepath}._PRT"):
            pkg = interpreter.interpret_method_call(f"{namepath}._PRT")
            prt = parse_pci_routing(pkg)
            prt_info = defaultdict(lambda: {})
            for mapping in prt:
                if isinstance(mapping.source, int):
                    assert mapping.source == 0, "A _PRT mapping package should not contain a byte of non-zero as source"
                    prt_info[mapping.address][mapping.pin] = mapping.source_index
                elif isinstance(mapping.source, context.DeviceDecl):
                    prt_info[mapping.address][mapping.pin] = (mapping.source.name, mapping.source_index)
                else:
                    logging.warning(f"The _PRT of {namepath} has a mapping with invalid source {mapping.source}")

            pin_routing_element = add_child(element, "interrupt_pin_routing")
            for address, pins in prt_info.items():
                mapping_element = add_child(pin_routing_element, "routing", address=hex(address))
                pin_names = {
                    0: "INTA#",
                    1: "INTB#",
                    2: "INTC#",
                    3: "INTD#",
                }
                for pin, info in pins.items():
                    if isinstance(info, int):
                        add_child(mapping_element, "mapping", pin=pin_names[pin], source=str(info))
                    else:
                        add_child(mapping_element, "mapping", pin=pin_names[pin], source=info[0], index=str(info[1]))

    except FutureWork:
        pass

def extract(board_etree):
    devices_node = get_node(board_etree, "//devices")

    try:
        namespace = parse_dsdt()
    except Exception as e:
        logging.warning(f"Parse ACPI DSDT/SSDT failed: {str(e)}")
        logging.warning(f"Will not extract information from ACPI DSDT/SSDT")
        return

    interpreter = ConcreteInterpreter(namespace)

    # With IOAPIC, Linux kernel will choose APIC mode as the IRQ model. Evalaute the \_PIC method (if exists) to inform the ACPI
    # namespace of this.
    try:
        interpreter.interpret_method_call("\\_PIC", 1)
    except:
        logging.info(f"\\_PIC is not evaluated.")

    for device in sorted(namespace.devices, key=lambda x:x.name):
        try:
            fetch_device_info(devices_node, interpreter, device.name)
        except Exception as e:
            logging.warning(f"Fetch information about device object {device.name} failed: {str(e)}")

    visitor = GenerateBinaryVisitor()
    for dev, objs in device_objects.items():
        element = get_node(devices_node, f"//device[acpi_object='{dev}']")
        if element is not None:
            scope = context.Context.parent(dev)
            name = dev[-4:]
            tree = builder.DefScope(
                builder.PkgLength(),
                scope,
                builder.TermList(
                    builder.DefDevice(
                        builder.PkgLength(),
                        name,
                        builder.TermList(*list(objs.values())))))
            add_child(element, "aml_template", visitor.generate(tree).hex())

    for dev, deps in device_deps.items():
        element = get_node(devices_node, f"//device[acpi_object='{dev}']")
        if element is not None:
            for kind, targets in deps.items():
                for target in targets:
                    if dev != target:
                        add_child(element, "dependency", target, type=kind)

advanced = True
