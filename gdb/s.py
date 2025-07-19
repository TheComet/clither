import gdb

class s_PrettyPrinter:
    def __init__(self, s):
        self.s = s

    def to_string(self):
        str_impl = self.s.cast(gdb.lookup_type("struct str_impl").pointer()).dereference()
        count = str_impl["count"] - 1
        capacity = str_impl["capacity"]
        s = gdb.Value(str_impl["data"]).cast(gdb.lookup_type("char").pointer()).string(length=count)
        return f"str(count={count}, capacity={capacity}) = \"{s}\""
