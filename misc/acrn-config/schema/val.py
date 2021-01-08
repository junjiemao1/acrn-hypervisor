#!/usr/bin/env python3

import argparse
import xmlschema
import lxml.etree as etree

parser = argparse.ArgumentParser()
parser.add_argument("schema", help="the XML schema in XSD to validate against")
parser.add_argument("xmlfile", help="the XML file to be validated")
args = parser.parse_args()

# XMLSchema does not process XInclude. Use lxml to expand the schema which is feed to XMLSchema as a string.
xsd_doc = etree.parse(args.schema)
xsd_doc.xinclude()
my_schema = xmlschema.XMLSchema11(etree.tostring(xsd_doc, encoding="unicode"))

it = my_schema.iter_errors(args.xmlfile)
for idx, validation_error in enumerate(it, start=1):
    print(f'[{idx}] path: {validation_error.path} | reason: {validation_error.reason}')
    print(validation_error)
