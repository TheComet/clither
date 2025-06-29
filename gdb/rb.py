class rb_PrettyPrinter:
    def __init__(self, rb):
        self.rb = rb

    def display_hint(self):
        return "array"

    def to_string(self):
        rb = self.rb.dereference()
        read = rb["read"]
        write = rb["write"]
        capacity = rb["capacity"]
        return f"rb(read={read}, write={write}, capacity={capacity})"

    def children(self):
        rb = self.rb.dereference()
        read = rb["read"]
        write = rb["write"]
        capacity = rb["capacity"]
        while read != write:
            data = rb["data"][read]
            yield ("0", data)
            read = (read + 1) % capacity

