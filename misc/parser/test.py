#!/usr/bin/env python3

import sys, traceback

from aml.stream import Stream
from aml.parser import AMLCode
from aml.tree import Tree, PrintLayoutVisitor

if __name__ == "__main__":
    f = open("DSDT", "rb")
    stream = Stream(f.read().decode("latin-1"))
    tree = Tree()

    try:
        AMLCode.match(stream, tree)
    except Exception as e:
        print(f"[{hex(stream.current)}]", e)
        # PrintLayoutVisitor().visit_topdown(tree)
        traceback.print_exception(*sys.exc_info())
