import gdb

class strview_PrettyPrinter:
    def __init__(self, strview):
        self.strview = strview

    def to_string(self):
        off = int(self.strview["off"])
        len_ = int(self.strview["len"])
        string = gdb.Value(self.strview["data"] + off)\
            .cast(gdb.lookup_type("char").pointer())\
            .string(length=len_)
        return f'strview(off={off}, len={len_}, "{string}")'
