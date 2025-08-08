import gdb

class rb_PrettyPrinter:
    def __init__(self, rb):
        self.rb = rb

    def display_hint(self):
        return "array"

    def to_string(self):
        if int(self.rb) == 0:
            return "rb(0)"
        rb = self.rb.dereference()
        read = rb["read"]
        write = rb["write"]
        capacity = rb["capacity"]
        count = (write - read) % capacity
        return f"rb(r={read}, w={write}, n={count}, cap={capacity})"

    def children(self):
        if int(self.rb) == 0:
            return
        rb = self.rb.dereference()
        read = rb["read"]
        write = rb["write"]
        capacity = rb["capacity"]
        while read != write:
            data = rb["data"][read]
            yield ("0", data)
            read = (read + 1) % capacity

