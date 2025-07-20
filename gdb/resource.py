import gdb

class resource_snake_PrettyPrinter:
    def __init__(self, res):
        self.res = res

    def to_string(self):
        if int(self.res) == 0:
            return "NULL"
        return self.res.dereference()

class resource_sprite_PrettyPrinter:
    def __init__(self, res):
        self.res = res

    def to_string(self):
        if int(self.res) == 0:
            return "NULL"
        return self.res.dereference()

class resource_pack_PrettyPrinter:
    def __init__(self, res):
        self.res = res

    def to_string(self):
        if int(self.res) == 0:
            return "NULL"
        return self.res.dereference()
