import gdb

class PStr(gdb.Command):
    """Print only the to_string() of a pretty printer, without children."""

    def __init__(self):
        super().__init__("pstr", gdb.COMMAND_USER)

    def invoke(self, arg, from_tty):
        val = gdb.parse_and_eval(arg)
        printer = gdb.default_visualizer(val)
        if printer is not None and hasattr(printer, "to_string"):
            print(printer.to_string())
        else:
            print(val)

PStr()
