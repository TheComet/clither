import gdb
import matplotlib.pyplot as plt
import numpy as np
from math import cos, sin

def qw_to_float(qw):
    Q = 14
    return float(qw) / (1 << Q)

def q16_16_to_float(qw):
    Q = 16
    return float(qw) / (1 << Q)

def qa_to_float(qa):
    Q = 12
    return float(qa) / (1 << Q)

def plot_knot(knot, color):
    x = qw_to_float(knot["pos"]["x"])
    y = qw_to_float(knot["pos"]["y"])
    a = qa_to_float(knot["angle"])
    lb = float(knot["len_backwards"]) / 255
    lf = float(knot["len_forwards"]) / 255
    fx = x - cos(a) * lf
    fy = y - sin(a) * lf
    bx = x + cos(a) * lb
    by = y + sin(a) * lb
    handle, = plt.plot([bx, fx], [by, fy], color=color, linewidth=0.5)
    plt.scatter(x, y, color=color, marker=",")
    plt.scatter(fx, fy, color=color, marker=",", s=10)
    plt.scatter(bx, by, color=color, marker=",", s=10)
    return handle

def plot_knots(rb, color):
    handle = None
    read = rb["read"]
    write = rb["write"]
    capacity = rb["capacity"]
    while read != write:
        knot = rb["data"][read]
        handle = plot_knot(knot, color)
        read = (read + 1) % capacity
    return handle

def plot_segment(segment, color):
    p = segment["p"]
    Ax = segment["coeff_x"]
    Ay = segment["coeff_y"]
    px = [qw_to_float(p[i]["x"]) for i in range(4)]
    py = [qw_to_float(p[i]["y"]) for i in range(4)]
    Ax = [q16_16_to_float(Ax[i]) for i in range(3)]
    Ay = [q16_16_to_float(Ay[i]) for i in range(3)]

    t = np.linspace(0, 1, 50)
    x = (Ax[0]*t + Ax[1]*t**2 + Ax[2]*t**3) + px[0]
    y = (Ay[0]*t + Ay[1]*t**2 + Ay[2]*t**3) + py[0]
    handle, = plt.plot(x, y, color=color)
    plt.plot([px[0], px[1]], [py[0], py[1]], color=color, linewidth=0.5)
    plt.plot([px[2], px[3]], [py[2], py[3]], color=color, linewidth=0.5)
    plt.scatter(px[0], py[0], color=color, marker=",")
    plt.scatter(px[3], py[3], color=color, marker=",")
    plt.scatter(px[1], py[1], color=color, marker=",", s=10)
    plt.scatter(px[2], py[2], color=color, marker=",", s=10)
    return handle

def plot_segments(rb, color):
    handle = None
    read = rb["read"]
    write = rb["write"]
    capacity = rb["capacity"]
    while read != write:
        segment = rb["data"][read]
        handle = plot_segment(segment, color=color)
        read = (read + 1) % capacity
    return handle

def plot_point(x, y, color):
    return plt.scatter([x], [y], color=color)

def plot_head_trail(vec, color):
    count = vec["count"]
    data = vec["data"]
    x, y = list(), list()
    for i in range(count):
        point = data[i]
        x.append(qw_to_float(point["x"]))
        y.append(qw_to_float(point["y"]))
    handle = plt.scatter(x, y, color=color)
    return handle

def plot_trails(vec_rb, color):
    handle = None
    read = vec_rb["read"]
    write = vec_rb["write"]
    capacity = vec_rb["capacity"]
    while read != write:
        handle = plot_head_trail(vec_rb["data"][read], color=color)
        read = (read + 1) % capacity
    return handle

def plot_aabb(qwaabb, color):
    x1 = qw_to_float(qwaabb["x1"])
    y1 = qw_to_float(qwaabb["y1"])
    x2 = qw_to_float(qwaabb["x2"])
    y2 = qw_to_float(qwaabb["y2"])
    handle, = plt.plot([x1, x2, x2, x1, x1], [y1, y1, y2, y2, y1], color=color, linewidth=0.5)
    return handle

def plot_segment_bbs(qwaabb_rb, color):
    handle = None
    read = qwaabb_rb["read"]
    write = qwaabb_rb["write"]
    capacity = qwaabb_rb["capacity"]
    while read != write:
        handle = plot_aabb(qwaabb_rb["data"][read], color=color)
        read = (read + 1) % capacity
    return handle

color_table = (
    ("#FF6060", "#FF8080", "#C02020"),
    ("#FF8000", "#FFA000", "#C07010"),
    ("#FF60FF", "#FFA0FF", "#C020C0"),
    ("#6060FF", "#A0A0FF", "#2020C0"),
    ("#60FFFF", "#A0FFFF", "#20C0C0"),
    ("#60FF60", "#A0FFA0", "#20C020"),
)

class Plot(gdb.Command):
    def __init__(self):
        super(Plot, self).__init__("plot", gdb.COMMAND_USER)

    def invoke(self, arg, from_tty):
        handle = None
        exprs = arg.split()
        values = (gdb.parse_and_eval(x) for x in exprs)
        for i, val in enumerate(values):
            colors = color_table[i % len(color_table)]
            if str(val.type).endswith("snake *"):
                val = val.dereference()
            if str(val.type).endswith("snake"):
                plot_trails(val["data"]["trails"], colors[0])
                plot_knots(val["data"]["knots"], colors[2])
                plot_segments(val["data"]["segments"], colors[2])
                plot_segment_bbs(val["data"]["segment_bbs"], colors[1])
                handle = plot_aabb(val["data"]["bb"], colors[1])

            if str(val.type).endswith("snake_data *"):
                val = val.dereference()
            if str(val.type).endswith("snake_data"):
                plot_trails(val["trails"], colors[0])
                plot_knots(val["knots"], colors[2])
                plot_segments(val["segments"], colors[2])
                plot_segment_bbs(val["segment_bbs"], colors[1])
                handle = plot_aabb(val["bb"], colors[1])

            if str(val.type).endswith("snake_head *"):
                val = val.dereference()
            if str(val.type).endswith("snake_head"):
                x = qw_to_float(val["pos"]["x"])
                y = qw_to_float(val["pos"]["y"])
                handle = plot_point(x, y, colors[0])

            if str(val.type).endswith("qwpos *"):
                val = val.dereference()
            if str(val.type).endswith("qwpos"):
                x = qw_to_float(val["x"])
                y = qw_to_float(val["y"])
                handle = plot_point(x, y, colors[0])

            if str(val.type).endswith("bezier_knot_rb *"):
                handle = plot_knots(val, colors[2])

            if str(val.type).endswith("bezier_knot *"):
                val = val.dereference()
            if str(val.type).endswith("bezier_knot"):
                handle = plot_knot(val, colors[2])

            if str(val.type).endswith("bezier_segment_rb *"):
                handle = plot_segments(val, colors[2])

            if str(val.type).endswith("bezier_segment *"):
                val = val.dereference()
            if str(val.type).endswith("bezier_segment"):
                handle = plot_segment(val, colors[2])

            if str(val.type).endswith("qwpos_vec_rb *"):
                handle = plot_trails(val, colors[0])

            if str(val.type).endswith("qwpos_vec *"):
                handle = plot_head_trail(val, colors[0])

            if str(val.type).endswith("qwaabb_rb *"):
                handle = plot_segment_bbs(val, colors[1])

            if str(val.type).endswith("qwaabb"):
                handle = plot_aabb(val, colors[1])

            if handle is not None:
                handle.set_label(exprs[i])

        plt.legend()
        plt.gca().set_aspect('equal', adjustable='box')
        plt.gca().set_facecolor("#011627")
        plt.show()

Plot()
