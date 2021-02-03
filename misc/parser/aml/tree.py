class Tree:
    def __init__(self):
        self.label = None
        self.children = []

    def append_child(self, child):
        self.children.append(child)

class Visitor:
    def __init__(self):
        self.depth = 0

    def __visit(self, tree):
        fn = getattr(self, tree.label, None)
        if not fn:
            fn = getattr(self, "default", None)
        if fn:
            fn(tree)

    def visit_topdown(self, tree):
        self.__visit(tree)
        if isinstance(tree.children, list):
            self.depth += 1
            for child in tree.children:
                if isinstance(child, Tree):
                    self.visit_topdown(child)
            self.depth -= 1

class PrintLayoutVisitor(Visitor):
    def default(self, tree):
        indent = "  " * self.depth
        if isinstance(tree.children, int):
            print(f"{indent}{tree.label} = {hex(tree.children)}")
        elif isinstance(tree.children, str):
            print(f"{indent}{tree.label} = '{tree.children}'")
        else:
            print(f"{indent}{tree.label}")

class Transformer:
    def __init__(self):
        self.depth = 0

    def __transform(self, tree):
        fn = getattr(self, tree.label, None)
        if not fn:
            fn = getattr(self, "default", None)
        if fn:
            return fn(tree)
        else:
            return tree

    def transform_bottomup(self, tree):
        if isinstance(tree.children, list):
            if self.depth < 10:
                self.depth += 1
                for i, child in enumerate(tree.children):
                    if isinstance(child, Tree):
                        tree.children[i] = self.transform_bottomup(child)
                self.depth -= 1
        self.__transform(tree)

class FlattenTransformer(Transformer):
    def TermObj(self, tree):
        if isinstance(tree.children, list) and \
           len(tree.children) == 1 and \
           isinstance(tree.children[0], list):
            tree.children = tree.children[0].children
        return tree
