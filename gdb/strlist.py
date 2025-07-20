import gdb

class strlist_PrettyPrinter:
    def __init__(self, strlist):
        self.strlist = strlist

    def display_hint(self):
        return "array"

    def to_string(self):
        if int(self.strlist) == 0:
            return "strlist(0) = {}"
        strlist = self.strlist.dereference()
        count = strlist["count"]
        capacity = strlist["capacity"]
        str_used = strlist["str_used"]
        return f"strlist(count={count}, capacity={capacity}, used={str_used})"

    def children(self):
        if int(self.strlist) == 0:
            return
        strlist = self.strlist.dereference()
        data = strlist["data"].address
        span_table = gdb.Value(data + int(strlist["capacity"]))\
            .cast(gdb.lookup_type("struct strspan").pointer())
        for i in range(int(strlist["count"])):
            span = gdb.Value(span_table - i - 1).dereference()
            string = gdb.Value(data + span["off"])\
                .cast(gdb.lookup_type("char").pointer())\
                .string(length=span["len"])
            yield (str(i), f'"{string}"')

