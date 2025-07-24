import gdb
import matplotlib.pyplot as plt
import numpy as np

def type_has_width_and_height(tp):
    required_fields = ("width", "height", "data")
    if tp.code == gdb.TYPE_CODE_PTR:
        tp = tp.target()
    if tp.code == gdb.TYPE_CODE_STRUCT:
        names = [x.name for x in tp.fields()]
        if ("width" in names or "w" in names) and \
           ("height" in names or "h" in names) and \
            "data" in names:
            return True
    return False

def args_to_image(args):
    if len(args) < 1:
        raise gdb.GdbError("Usage: img <pixel data> [<width> <height>] [r|g|b|a]")
    val = gdb.parse_and_eval(args[0])

    if type_has_width_and_height(val.type.strip_typedefs()):
        if val.type.code == gdb.TYPE_CODE_PTR:
            val = val.dereference()
        channels = args[1] if len(args) > 1 else None
        return val["width"], val["height"], val["data"], channels
    else:
        if len(args) < 3:
            raise gdb.GdbError("Require <width> and <height> arguments "
                "when pixel data does not have width and height fields.")
        width = gdb.parse_and_eval(args[1])
        height = gdb.parse_and_eval(args[2])
        channels = args[3] if len(args) > 3 else None
        return int(width), int(height), val, channels

def filter_channels(data, channels):
    if channels is not None:
        if len(channels) == 1:
            channel_count = 1
        elif len(channels) == 2:
            channel_count = 3
        elif len(channels) == 3:
            channel_count = 3
        elif len(channels) == 4:
            channel_count = 4

        new_data = np.zeros(
            (data.shape[0], data.shape[1], channel_count),
            dtype=np.uint8
        )
        for i, channel in enumerate(channels):
            print(channel)
            if channel == "r":
                new_data[:, :, i] = data[:, :, 0]
            elif channel == "g":
                new_data[:, :, i] = data[:, :, 1]
            elif channel == "b":
                new_data[:, :, i] = data[:, :, 2]
            elif channel == "a":
                new_data[:, :, i] = data[:, :, 3]
            else:
                raise gdb.GdbError(f"Unknown channel '{channel}'")
        return new_data
    return data

class R8(gdb.Command):
    def __init__(self):
        super(R8, self).__init__("r8", gdb.COMMAND_USER)

    def invoke(self, arg, from_tty):
        args = arg.split()
        width, height, data, channels = args_to_image(args)
        data = gdb.selected_inferior().read_memory(data, width * height)
        data = np.frombuffer(data, dtype=np.uint8).reshape((height, width))
        data = filter_channels(data, channels)

        plt.imshow(data, cmap="gray")
        plt.gca().set_aspect("equal", adjustable="box")
        plt.show()

class RGB(gdb.Command):
    def __init__(self):
        super(RGB, self).__init__("rgb", gdb.COMMAND_USER)

    def invoke(self, arg, from_tty):
        args = arg.split()
        width, height, data, channels = args_to_image(args)
        data = gdb.selected_inferior().read_memory(data, width * height * 3)
        data = np.frombuffer(data, dtype=np.uint8).reshape((height, width, 3))
        data = filter_channels(data, channels)

        if data.shape[2] == 1:
            plt.imshow(data, cmap="gray")
        else:
            plt.imshow(data)
        plt.gca().set_aspect("equal", adjustable="box")
        plt.show()

class RGBA(gdb.Command):
    def __init__(self):
        super(RGBA, self).__init__("rgba", gdb.COMMAND_USER)

    def invoke(self, arg, from_tty):
        args = arg.split()
        width, height, data, channels = args_to_image(args)
        data = gdb.selected_inferior().read_memory(data, width * height * 4)
        data = np.frombuffer(data, dtype=np.uint8).reshape((height, width, 4))
        data = filter_channels(data, channels)

        if data.shape[2] == 1:
            plt.imshow(data, cmap="gray")
        else:
            plt.imshow(data)
        plt.gca().set_aspect("equal", adjustable="box")
        plt.show()

R8()
RGB()
RGBA()

