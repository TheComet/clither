class strspan_PrettyPrinter:
    def __init__(self, strspan):
        self.strspan = strspan

    def to_string(self):
        off = int(self.strspan["off"])
        len_ = int(self.strspan["len"])
        return f"strspan({off}, {len_})"

