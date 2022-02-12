#!/usr/bin/env python3
#
# Copyright (C) 2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import os
import argparse
from copy import deepcopy

from pipeline import PipelineObject, PipelineStage, PipelineEngine

class SchemaTypeSlicer:
    xpath_ns = {
        "xs": "http://www.w3.org/2001/XMLSchema",
        "acrn": "https://projectacrn.org",
    }

    @classmethod
    def get_node(cls, element, xpath):
        return element.find(xpath, namespaces=cls.xpath_ns)

    @classmethod
    def get_nodes(cls, element, xpath):
        return element.findall(xpath, namespaces=cls.xpath_ns)

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

        all_elements_node = self.get_node(new_type_node, "xs:all")
        if all_elements_node is not None:
            for element_node in list(self.get_nodes(all_elements_node, "xs:element")):
                if not self.is_element_needed(element_node):
                    all_elements_node.remove(element_node)
                    sliced = True
                    continue

                # For embedded complex type definition, also slice in place. If the sliced type contains no sub-element,
                # remove the element itself, too.
                element_type_node = self.get_node(element_node, "xs:complexType")
                if element_type_node is not None:
                    new_sub_nodes = self.slice(element_type_node, in_place=True)
                    if len(self.get_nodes(element_type_node, ".//xs:element")) > 0:
                        new_nodes.extend(new_sub_nodes)
                    else:
                        all_elements_node.remove(element_node)
                    continue

                # For external complex type definition, create a copy to slice. If the sliced type contains no
                # sub-element, remove the element itself.
                element_type_name = element_node.get("type")
                if element_type_name:
                    element_type_node = self.get_node(self.etree, f"//xs:complexType[@name='{element_type_name}']")
                    if element_type_node is not None:
                        new_sub_nodes = self.slice(element_type_node)
                        if len(new_sub_nodes) == 0:
                            continue
                        elif len(self.get_nodes(new_sub_nodes[-1], ".//xs:element")) > 0:
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

class SlicingSchemaByVMTypeStage(PipelineStage):
    uses = {"schema_etree"}
    provides = {"schema_etree"}

    class VMTypeSlicer(SchemaTypeSlicer):
        def is_element_needed(self, element_node):
            annot_node = self.get_node(element_node, "xs:annotation")
            if annot_node is None:
                return True
            applicable_vms = annot_node.get("{https://projectacrn.org}applicable-vms")
            return applicable_vms is None or applicable_vms.find(self.vm_type_indicator) >= 0

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

    def run(self, obj):
        schema_etree = obj.get("schema_etree")

        vm_type_name = "VMConfigType"
        vm_type_node = SchemaTypeSlicer.get_node(schema_etree, f"//xs:complexType[@name='{vm_type_name}']")
        slicers = [
            self.PreLaunchedTypeSlicer(schema_etree),
            self.ServiceVMTypeSlicer(schema_etree),
            self.PostLaunchedTypeSlicer(schema_etree)
        ]

        for slicer in slicers:
            new_nodes = slicer.slice(vm_type_node, force_copy=True)
            for n in new_nodes:
                schema_etree.getroot().append(n)

        for node in SchemaTypeSlicer.get_nodes(schema_etree, "//xs:complexType[@name='ACRNConfigType']//xs:element[@name='vm']//xs:alternative"):
            test = node.get("test")
            if test.find("PRE_LAUNCHED_VM") >= 0:
                node.set("type", slicers[0].get_name_of_slice(vm_type_name))
            elif test.find("SERVICE_VM") >= 0:
                node.set("type", slicers[1].get_name_of_slice(vm_type_name))
            elif test.find("POST_LAUNCHED_VM") >= 0:
                node.set("type", slicers[2].get_name_of_slice(vm_type_name))

        obj.set("schema_etree", schema_etree)

class SlicingSchemaByViewStage(PipelineStage):
    uses = {"schema_etree"}
    provides = {"schema_etree"}

    class ViewSlicer(SchemaTypeSlicer):
        def is_element_needed(self, element_node):
            annot_node = self.get_node(element_node, "xs:annotation")
            if annot_node is None:
                return False
            views = annot_node.get("{https://projectacrn.org}views")
            return views is not None and views.find(self.view_indicator) >= 0

        def get_name_of_slice(self, name):
            if name.find("ConfigType") >= 0:
                return name.replace("ConfigType", f"{self.type_prefix}ConfigType")
            else:
                return f"{self.type_prefix}{name}"

    class BasicViewSlicer(ViewSlicer):
        view_indicator = "basic"
        type_prefix = "Basic"

    class AdvancedViewSlicer(ViewSlicer):
        view_indicator = "advanced"
        type_prefix = "Advanced"

    def run(self, obj):
        schema_etree = obj.get("schema_etree")

        type_nodes = list(filter(lambda x: x.get("name") and x.get("name").endswith("VMConfigType"), SchemaTypeSlicer.get_nodes(schema_etree, "//xs:complexType")))
        type_nodes.append(SchemaTypeSlicer.get_node(schema_etree, "//xs:complexType[@name = 'HVConfigType']"))

        slicers = [
            self.BasicViewSlicer(schema_etree),
            self.AdvancedViewSlicer(schema_etree),
        ]

        for slicer in slicers:
            for type_node in type_nodes:
                new_nodes = slicer.slice(type_node, force_copy=True)
                for n in new_nodes:
                    schema_etree.getroot().append(n)

        obj.set("schema_etree", schema_etree)

def main(args):
    from lxml_loader import LXMLLoadStage

    pipeline = PipelineEngine(["schema_path"])
    pipeline.add_stages([
        LXMLLoadStage("schema"),
        SlicingSchemaByVMTypeStage(),
        SlicingSchemaByViewStage(),
    ])

    obj = PipelineObject(schema_path = args.schema)
    pipeline.run(obj)
    obj.get("schema_etree").write(args.out)

    print(f"Sliced schema written to {args.out}")

if __name__ == "__main__":
    config_tools_dir = os.path.join(os.path.dirname(__file__), "..")
    schema_dir = os.path.join(config_tools_dir, "schema")

    parser = argparse.ArgumentParser(description="Slice a given scenario schema by VM types and views")
    parser.add_argument("out", nargs="?", default=os.path.join(schema_dir, "sliced.xsd"), help="Path where the output is placed")
    parser.add_argument("--schema", default=os.path.join(schema_dir, "config.xsd"), help="the XML schema that defines the syntax of scenario XMLs")
    args = parser.parse_args()

    main(args)
