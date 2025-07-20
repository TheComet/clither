import gdb

class vec_PrettyPrinter:
    def __init__(self, vec):
        self.vec = vec

    def display_hint(self):
        return "array"

    def to_string(self):
        if int(self.vec) == 0:
            return "vec(0)"
        vec = self.vec.dereference()
        count = vec["count"]
        capacity = vec["capacity"]
        return f"vec(count={count}, capacity={capacity})"

    def children(self):
        if int(self.vec) == 0:
            return
        vec = self.vec.dereference()
        count = vec["count"]
        data = vec["data"]
        for i in range(count):
            yield (str(i), data[i])

