#!/usr/bin/env python3

import sys, os
import logging
logging.basicConfig(level=logging.INFO)

from acpiparser import parse_dsdt
from acpiparser.aml.interpreter import ConcreteInterpreter
from acpiparser.aml.visitors import PrintLayoutVisitor, GenerateBinaryVisitor
from acpiparser.rdt import parse_resource_data

# namespace = parse_dsdt()
# interpreter = ConcreteInterpreter(namespace)
# result = interpreter.interpret_method_call(f"\_SB_.PCI0.LPCB.UAR1._CRS")
# print(parse_resource_data(result.get()))

curdir = os.path.dirname(sys.argv[0])
path = "DSDT.aml"
namespace = parse_dsdt(path=os.path.join(curdir, path))
visitor = GenerateBinaryVisitor()
visitor.visit(namespace.trees[path])
result = visitor.get_result()
orig = open(os.path.join(curdir, path), 'rb').read()

for i in range(len(orig)):
    if i < len(result):
        if orig[i] != result[i]:
            print(f"Byte {i}: {orig[i]} vs. {result[i]}")
            break

f = open(os.path.join(curdir, "result.dat"), 'wb')
f.write(result)
f.close()

# decl = namespace.lookup_symbol("\\UAR0")
# PrintLayoutVisitor().visit_topdown(namespace.trees[path])

# interpreter = ConcreteInterpreter(namespace)
# result = interpreter.interpret_method_call(f"\FOO_")
# print(result.get())

# decl = namespace.lookup_symbol(f"{namepath}._INI")
# decl.dump()
# PrintLayoutVisitor().visit_topdown(decl.tree)

# sta = None
# if interpreter.context.has_symbol(f"{namepath}._STA"):
#     sta = interpreter.interpret_method_call(f"{namepath}._STA").get()
#     if sta & 0x1 == 0:
#         print("skip")

# import pcieparser
# print(pcieparser.parse_config_space("/sys/devices/pci0000:00/0000:00:00.0"))

# import acpiparser
# print(acpiparser.parse_rtct(path="/tmp/PTCT.tcc-mode-enabled"))

# import cpuparser
# c = cpuparser.cpuids.LEAF_1.read(4)
# print(c.fpu)

# import smbiosparser
# c = smbiosparser.SMBIOS()
# print(c)

# import memmapparser
# t = memmapparser.parse_e820()
# print(t)
