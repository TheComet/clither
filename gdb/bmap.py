import gdb

class bmap_PrettyPrinter:
    def __init__(self, bmap):
        self.bmap = bmap

    def display_hint(self):
        return "map"

    def to_string(self):
        if int(self.bmap) == 0:
            return "bmap(0)"
        bmap = self.bmap.dereference()
        count = int(bmap["count"])
        capacity = int(bmap["capacity"])
        return f"bmap(count={count}, capacity={capacity})"

    def children(self):
        if int(self.bmap) == 0:
            return
        bmap = self.bmap.dereference()
        for i in range(int(bmap["count"])):
            key = gdb.Value(bmap["keys"] + i).dereference()
            value = gdb.Value(bmap["values"].address + i).dereference()
            yield (str(i), key)
            yield (str(i), value)
