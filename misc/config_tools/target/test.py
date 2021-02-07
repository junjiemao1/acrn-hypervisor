#!/usr/bin/env python3

import sys, traceback
import logging

from acpiparser.aml.stream import Stream
from acpiparser.aml.parser import AMLCode, DeferredExpansion
from acpiparser.aml.tree import Tree, PrintLayoutVisitor, FlattenTransformer
from acpiparser.aml.context import Context
from acpiparser.aml.interpreter import ConcreteInterpreter
from acpiparser.rdt import parse_resource_data

def print_binary(stream, base):
    acc = f"[{hex(base)}/{hex(len(stream.data))}]"
    converted = ""
    for i in range(base, base + 16):
        code = ord(stream.data[i])
        acc += " %02x" % code
        if code >= 0x20 and code <= 0x7E:
            converted += chr(code)
        else:
            converted += "."
        if i > base and ((i - base) % 4) == 3:
            acc += " "
            converted += " "
    acc += f"    '{converted}'"
    print(acc)

if __name__ == "__main__":
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

    logging.basicConfig(level=logging.INFO)

    ctx = Context()
    forest = []
    try:
        for t in tables:
            logging.info(f"Loading {t}")
            ctx.switch_stream(t)
            tree = Tree()
            AMLCode.parse(ctx, tree)
            tree = DeferredExpansion(ctx).transform_topdown(tree)
            tree = FlattenTransformer().transform_bottomup(tree)
            forest.append(tree)
    except Exception as e:
        # PrintLayoutVisitor().visit_topdown(tree)
        traceback.print_exception(*sys.exc_info())
        ctx.current_stream.dump()
        # tree.context.dump_symbols()
        sys.exit(1)

    # logging.getLogger().setLevel(logging.DEBUG)
    interpreter = ConcreteInterpreter(ctx)
    try:
        result = interpreter.interpret_method_call("\\_SB_.PCI0.LPCB.PS2K._STA")
        print(result.get())
        # result = ConcreteInterpreter(ctx).interpret_method_call("\\_SB_.PCI0.LPCB.PS2M._CRS")
        # result = ConcreteInterpreter(ctx).interpret_method_call("\\_SB_.PCI0.LPCB.SIO1._CRS")
        # result = ConcreteInterpreter(ctx).interpret_method_call("\\_SB_.PCI0._CRS")
    except:
        raise
    finally:
        pass
    interpreter.dump()
    # parsed = parse_resource_data(result.data)
    # print(parsed)
