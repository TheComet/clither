import gdb

class hmap_PrettyPrinter:
    def __init__(self, hmap):
        self.hmap = hmap

    def display_hint(self):
        return "map"

    def to_string(self):
        if int(self.hmap) == 0:
            return "hmap(0)"
        hmap = self.hmap.dereference()
        count = int(hmap["count"])
        capacity = int(hmap["capacity"])
        return f"hmap(count={count}, capacity={capacity})"

    def children(self):
        if int(self.hmap) == 0:
            return
        hmap = self.hmap.dereference()
        hashes = hmap["hashes"].address
        capacity = int(hmap["capacity"])
        for i in range(capacity):
            hash = (hashes+i)\
                .cast(gdb.lookup_type("unsigned int").pointer())\
                .dereference()
            if hash <= 1:
                continue
            key = gdb.Value(hmap["kvs"]["keys"] + i).dereference()
            value = gdb.Value(hmap["kvs"]["values"] + i).dereference()
            yield (str(i), key)
            yield (str(i), value)
