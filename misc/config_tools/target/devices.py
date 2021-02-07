#!/usr/bin/env python3

import sys, traceback
import logging
import lxml.etree

from acpiparser.aml.stream import Stream
from acpiparser.aml.parser import AMLCode, DeferredExpansion
from acpiparser.aml.tree import Tree, PrintLayoutVisitor, FlattenTransformer
from acpiparser.aml.context import Context
from acpiparser.aml.interpreter import ConcreteInterpreter
from acpiparser.aml.exception import UndefinedSymbol, FutureWork
from acpiparser.rdt import *

predefined_nameseg = {
    "_SB_": "systembus",
    "_TZ_": "thermalzone",
}

def add_child(element, tag, text=None, **kwargs):
    child = lxml.etree.Element(tag)
    child.text = text
    for k,v in kwargs.items():
        child.set(k, v)
    element.append(child)
    return child

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

buses = {
    "PNP0A03": "pci",
    "PNP0A08": "pcie",
}

def get_device_element(devices_node, namepath):
    assert namepath.startswith("\\")
    namesegs = namepath[1:].split(".")

    element = devices_node
    for nameseg in namesegs:
        tag = "dev"
        if nameseg in predefined_nameseg.keys():
            tag = predefined_nameseg[nameseg]
        next_element = None
        for child in element:
            if child.get("object") == nameseg:
                next_element = child
                break
        if next_element is None:
            next_element = add_child(element, tag, None, object=nameseg)
        element = next_element

    return element

def parse_irq(item, elem):
    add_child(elem, "irq", int=hex(item._INT))

def parse_io_port(item, elem):
    add_child(elem, "pio", min=hex(item._MIN), max=hex(item._MAX), len=hex(item._LEN))

def parse_fixed_io_port(item, elem):
    add_child(elem, "pio", min=hex(item._BAS), max=hex(item._BAS + item._LEN - 1), len=hex(item._LEN))

def parse_fixed_memory_range(item, elem):
    add_child(elem, "mmio", min=hex(item._BAS), max=hex(item._BAS + item._LEN - 1), len=hex(item._LEN))

def parse_address_space_resource(item, elem):
    if item._TYP == 0:
        tag = "mmio"
    elif item._TYP == 1:
        tag = "pio"
    elif item._TYP == 2:
        tag = "bus_number"
    else:
        tag = "custom"
    add_child(elem, tag, min=hex(item._MIN), max=hex(item._MAX), len=hex(item._LEN))

def parse_extended_irq(item, elem):
    for irq in item._INT:
        add_child(elem, "irq", int=hex(irq))

resource_parsers = {
    (0, SMALL_RESOURCE_ITEM_IRQ_FORMAT): parse_irq,
    (0, SMALL_RESOURCE_ITEM_IO_PORT): parse_io_port,
    (0, SMALL_RESOURCE_ITEM_FIXED_LOCATION_IO_PORT): parse_fixed_io_port,
    (0, SMALL_RESOURCE_ITEM_END_TAG): (lambda x,y: None),
    (1, LARGE_RESOURCE_ITEM_32BIT_FIXED_MEMORY_RANGE): parse_fixed_memory_range,
    (1, LARGE_RESOURCE_ITEM_ADDRESS_SPACE_RESOURCE): parse_address_space_resource,
    (1, LARGE_RESOURCE_ITEM_WORD_ADDRESS_SPACE): parse_address_space_resource,
    (1, LARGE_RESOURCE_ITEM_EXTENDED_INTERRUPT): parse_extended_irq,
    (1, LARGE_RESOURCE_ITEM_QWORD_ADDRESS_SPACE): parse_address_space_resource,
    (1, LARGE_RESOURCE_ITEM_EXTENDED_ADDRESS_SPACE): parse_address_space_resource,
}

def fetch_device_info(devices_node, interpreter, namepath):
    logging.info(f"Fetch information about device object {namepath}")

    # Create the XML element for the device and create its ancestors if necessary
    element = get_device_element(devices_node, namepath)

    try:
        # Check if an _INI method exists
        try:
            interpreter.interpret_method_call(namepath + "._INI")
            add_child(element, "has_ini", "1")
        except UndefinedSymbol:
            add_child(element, "has_ini", "0")

        # Hardware ID
        if interpreter.context.has_symbol(f"{namepath}._HID"):
            hid = interpreter.interpret_method_call(f"{namepath}._HID").get()
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
            if hid in buses.keys():
                element.tag = buses[hid]
                buses.pop(hid)
            add_child(element, "hid", hid)

        # Address
        if interpreter.context.has_symbol(f"{namepath}._ADR"):
            adr = interpreter.interpret_method_call(f"{namepath}._ADR").get()
            add_child(element, "address", hex(adr) if isinstance(adr, int) else adr)

        # Status
        if interpreter.context.has_symbol(f"{namepath}._STA"):
            sta = interpreter.interpret_method_call(f"{namepath}._STA").get()
            status = add_child(element, "status")

            add_child(status, "present", "y" if sta & 0x1 != 0 else "n")
            add_child(status, "enabled", "y" if sta & 0x2 != 0 else "n")
            add_child(status, "functioning", "y" if sta & 0x8 != 0 else "n")

        # Resources
        if interpreter.context.has_symbol(f"{namepath}._CRS"):
            data = interpreter.interpret_method_call(f"{namepath}._CRS").get()
            rdt = parse_resource_data(data)
            resources = add_child(element, "resources")

            for item in rdt.items:
                p = (item.type, item.name)
                if p in resource_parsers.keys():
                    resource_parsers[p](item, resources)
                else:
                    add_child(resources, "unknown", name=item.__class__.__name__)

            # print(namepath)
            # print(rdt)

        # PCI interrupt routing
        if interpreter.context.has_symbol(f"{namepath}._PRT"):
            interpreter.interpret_method_call(f"{namepath}._PRT")

    except FutureWork:
        pass

def fetch_devices_info(etree):
    # Create the devices node
    root_node = etree.getroot()
    devices_node = lxml.etree.Element("devices")
    root_node.append(devices_node)

    # Load DSDT and SSDT tables
    tables = [
        "DSDT",
        "SSDT1",
        "SSDT2",
        "SSDT3",
        "SSDT4",
        "SSDT5",
        "SSDT6",
        "SSDT7",
        "SSDT8",
        "SSDT9",
        "SSDT10",
    ]
    context = Context()
    try:
        for t in tables:
            logging.info(f"Loading {t}")
            context.switch_stream(t)
            tree = Tree()
            AMLCode.parse(context, tree)
            tree = DeferredExpansion(context).transform_topdown(tree)
            tree = FlattenTransformer().transform_bottomup(tree)
    except Exception as e:
        traceback.print_exception(*sys.exc_info())
        context.current_stream.dump()
        raise

    interpreter = ConcreteInterpreter(context)
    for device in sorted(context.devices, key=lambda x:x.name):
        try:
            fetch_device_info(devices_node, interpreter, device.name)
        except:
            interpreter.dump()
            raise

if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    etree = lxml.etree.ElementTree(element=lxml.etree.fromstring("<acrn-config></acrn-config>"))
    try:
        fetch_devices_info(etree)
    finally:
        etree.write("board_info.xml", pretty_print=True)
