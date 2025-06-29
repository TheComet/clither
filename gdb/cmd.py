import gdb

class cmd_PrettyPrinter:
    def __init__(self, cmd):
        self.cmd = cmd

    def to_string(self):
        angle = int(self.cmd["angle"])
        speed = int(self.cmd["speed"])
        action = gdb.Value(self.cmd["action"])\
            .cast(gdb.lookup_type("enum cmd_action"))
        return f"cmd(angle={angle}, speed={speed}, action={action})"
