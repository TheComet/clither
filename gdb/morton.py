import gdb

class morton_PrettyPrinter:
    def __init__(self, morton):
        self.morton = morton

    def to_string(self):
        expr = f"(struct qwpos) morton_decode_qwpos({int(self.morton)})"
        qwpos = gdb.parse_and_eval(expr)
        return str(qwpos)
