#!/usr/bin/env python3

import os
import lxml.etree as etree
from copy import deepcopy

xpath_ns = {
    "xs": "http://www.w3.org/2001/XMLSchema",
    "acrn": "https://projectacrn.org",
}

def get_node(element, xpath):
    return next(iter(element.xpath(xpath, namespaces=xpath_ns)), None)

class SchemaTypeSlicer:
    def __init__(self, etree):
        self.etree = etree

    def slice(self, type_node, in_place=False, force_copy=False):
        new_nodes = []
        sliced = False

        if in_place:
            new_type_node = type_node
        else:
            new_type_node = deepcopy(type_node)
            type_name = type_node.get("name")
            if type_name != None:
                new_type_node.set("name", self.get_name_of_slice(type_name))

        all_elements_node = new_type_node.find("xs:all", namespaces=xpath_ns)
        if all_elements_node is not None:
            for element_node in list(all_elements_node.findall("xs:element", namespaces=xpath_ns)):
                if not self.is_element_needed(element_node):
                    all_elements_node.remove(element_node)
                    sliced = True
                    continue

                # For embedded complex type definition, also slice in place. If the sliced type contains no sub-element,
                # remove the element itself, too.
                element_type_node = element_node.find("xs:complexType", namespaces=xpath_ns)
                if element_type_node is not None:
                    new_sub_nodes = self.slice(element_type_node, in_place=True)
                    if element_type_node.xpath("count(.//xs:element)", namespaces=xpath_ns) > 0:
                        new_nodes.extend(new_sub_nodes)
                    else:
                        all_elements_node.remove(element_node)
                    continue

                # For external complex type definition, create a copy to slice. If the sliced type contains no
                # sub-element, remove the element itself.
                element_type_name = element_node.get("type")
                if element_type_name:
                    element_type_node = get_node(self.etree, f"//xs:complexType[@name='{element_type_name}']")
                    if element_type_node is not None:
                        new_sub_nodes = self.slice(element_type_node)
                        if len(new_sub_nodes) == 0:
                            continue
                        elif new_sub_nodes[-1].xpath("count(.//xs:element)", namespaces=xpath_ns) > 0:
                            new_nodes.extend(new_sub_nodes)
                            element_node.set("type", self.get_name_of_slice(element_type_name))
                            sliced = True
                        else:
                            all_elements_node.remove(element_node)

        if not in_place and (sliced or force_copy):
            new_nodes.append(new_type_node)
        return new_nodes

    def is_element_needed(self, element_node):
        return True

    def get_name_of_slice(self, name):
        return f"Sliced{name}"

class VMTypeSlicer(SchemaTypeSlicer):
    def is_element_needed(self, element_node):
        applicable_vms = get_node(element_node, "xs:annotation/@acrn:applicable-vms")
        if applicable_vms is None:
            return True
        return applicable_vms.find(self.vm_type_indicator) >= 0

    def get_name_of_slice(self, name):
        return f"{self.type_prefix}{name}"

class PreLaunchedTypeSlicer(VMTypeSlicer):
    vm_type_indicator = "pre-launched"
    type_prefix = "PreLaunched"

class ServiceVMTypeSlicer(VMTypeSlicer):
    vm_type_indicator = "service-vm"
    type_prefix = "Service"

class PostLaunchedTypeSlicer(VMTypeSlicer):
    vm_type_indicator = "post-launched"
    type_prefix = "PostLaunched"

def slice_schema(schema_etree):
    vm_type_name = "VMConfigType"
    vm_type_node = schema_etree.xpath(f"//xs:complexType[@name='{vm_type_name}']", namespaces=xpath_ns)[0]
    slicers = [PreLaunchedTypeSlicer(schema_etree), ServiceVMTypeSlicer(schema_etree), PostLaunchedTypeSlicer(schema_etree)]
    for slicer in slicers:
        new_nodes = slicer.slice(vm_type_node, force_copy=True)
        for n in new_nodes:
            schema_etree.getroot().append(n)

    for node in schema_etree.xpath("//xs:complexType[@name='ACRNConfigType']//xs:element[@name='vm']//xs:alternative", namespaces=xpath_ns):
        test = node.get("test")
        if test.find("PRE_LAUNCHED_VM") >= 0:
            node.set("type", slicers[0].get_name_of_slice(vm_type_name))
        elif test.find("SERVICE_VM") >= 0:
            node.set("type", slicers[1].get_name_of_slice(vm_type_name))
        elif test.find("POST_LAUNCHED_VM") >= 0:
            node.set("type", slicers[2].get_name_of_slice(vm_type_name))

if __name__ == "__main__":
    config_tools_dir = os.path.join(os.path.dirname(__file__), "..")
    schema_dir = os.path.join(config_tools_dir, "schema")

    etree = etree.parse(os.path.join(schema_dir, "config.xsd"))
    etree.xinclude()
    slice_schema(etree)
    etree.write(os.path.join(schema_dir, "sliced.xsd"))
    print(f"Sliced schema written to {os.path.join(schema_dir, 'sliced.xsd')}")
