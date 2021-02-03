from . import grammar
from .tree import Tree

class Factory:
    def match(self, stream, tree):
        raise NotImplementedError

    def opcodes(self):
        raise NotImplementedError

class ConstDataFactory(Factory):
    def __init__(self, label, width):
        self.label = label
        self.width = width

    def match(self, stream, tree):
        tree.label = self.label
        tree.children = stream.get_integer(self.width)
        return tree

    def opcodes(self):
        return None

ByteData = ConstDataFactory("ByteData", 1)
WordData = ConstDataFactory("WordData", 2)
DWordData = ConstDataFactory("DWordData", 4)
TWordData = ConstDataFactory("TWordData", 6)
QWordData = ConstDataFactory("QWordData", 8)

class NameStringFactory(Factory):
    def match(self, stream, tree):
        acc = ""

        # Namespace prefixes
        char = stream.get_char()
        while char in ["\\", "^"]:
            acc += char
            char = stream.get_char()

        # Object name
        if ord(char) == grammar.AML_DUAL_NAME_PREFIX:
            if acc:
                acc += "."
            acc += stream.get_fixed_length_string(4)
            acc += "."
            acc += stream.get_fixed_length_string(4)
        elif ord(char) == grammar.AML_MULTI_NAME_PREFIX:
            seg_count = stream.get_integer(1)
            for i in range(0, seg_count):
                if acc:
                    acc += "."
                acc += stream.get_fixed_length_string(4)
        elif char == "\x00":    # NullName
            pass
        else:                   # NameSeg
            stream.seek(-1)
            acc += stream.get_fixed_length_string(4)

        tree.label = "NameString"
        tree.children = acc

NameString = NameStringFactory()

class PkgLengthFactory(Factory):
    @staticmethod
    def get_package_length(byte_count, value):
        if byte_count == 0:
            total_size = (value & 0x3F)
        else:
            total_size = value & 0x0F
            for i in range(1, byte_count + 1):
                byte = (value & (0xFF << (i * 8))) >> (i * 8)
                total_size |= (byte << (i * 8 - 4))
        return total_size

    def match(self, stream, tree):
        pkg_lead_byte = stream.peek_integer(1)
        byte_count = pkg_lead_byte >> 6
        assert byte_count <= 3

        tree.label = "PkgLength"
        tree.children = stream.get_integer(byte_count + 1)

        print(self.get_package_length(byte_count, tree.children) - byte_count - 1)
        stream.push_scope(self.get_package_length(byte_count, tree.children) - byte_count - 1)
        return tree

PkgLength = PkgLengthFactory()

class SequenceFactory(Factory):
    def __init__(self, label, seq):
        self.label = label
        self.seq = seq
        self.opcode = []
        self.opcode_width = 0
        if isinstance(seq[0], int):
            self.opcode.append(seq[0])
            self.opcode_width = 1 if seq[0] <= 0xFF else 2
            self.seq = seq[1:]

    def match(self, stream, tree):
        stream.seek(self.opcode_width)
        tree.label = self.label
        for elem in self.seq:
            if elem.endswith("*"):
                elem = elem[:-1]
                factory = globals()[elem]
                while not stream.at_end():
                    child = Tree()
                    tree.append_child(child)
                    factory.match(stream, child)
                stream.pop_scope()
            else:
                factory = globals()[elem]
                child = Tree()
                tree.append_child(child)
                factory.match(stream, child)
        return tree

    def opcodes(self):
        return self.opcode

class OptionFactory(Factory):
    def __init__(self, label, opts):
        self.label = label
        self.opts = opts
        self.__opcodes = None

    def match(self, stream, tree):
        opcode = stream.peek_integer(1)
        if opcode == grammar.AML_EXT_OP_PREFIX:
            opcode = stream.peek_integer(2)

        for opt in self.opts:
            factory = globals()[opt]
            matched_opcodes = factory.opcodes()
            if matched_opcodes is None or opcode in matched_opcodes:
                tree.label = self.label
                child = Tree()
                tree.append_child(child)
                factory.match(stream, child)
                return tree

        assert False, f"{hex(opcode)} is not a known opcode for {self.label}"

    def opcodes(self):
        if not self.__opcodes:
            self.__opcodes = []
            for opt in self.opts:
                self.__opcodes.extend(globals()[opt].opcodes())
        return self.__opcodes

for sym in dir(grammar):
    if sym.startswith("__") or (sym.upper() == sym):
        continue
    definition = getattr(grammar, sym)
    if isinstance(definition, tuple):
        globals()[sym] = SequenceFactory(sym, definition)
    elif isinstance(definition, list):
        globals()[sym] = OptionFactory(sym, definition)

def parse(stream):
    ret = Tree()
    AMLCode.match(stream, ret)
    return ret
