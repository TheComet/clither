import gdb
import matplotlib.pyplot as plt
import numpy as np
from math import cos, sin

def qw_to_float(qw):
    Q = 14
    return float(qw) / (1 << Q)

def qa_to_float(qa):
    Q = 12
    return float(qa) / (1 << Q)

def iter_knots(rb):
    read = rb["read"]
    write = rb["write"]
    capacity = rb["capacity"]
    while read != write:
        knot = rb["data"][read]
        yield \
            qw_to_float(knot["pos"]["x"]),\
            qw_to_float(knot["pos"]["y"]),\
            qa_to_float(knot["angle"]),\
            float(knot["len_backwards"]) / 255,\
            float(knot["len_forwards"]) / 255
        read = (read + 1) % capacity

def plot_knots(rb, color):
    knots = list(iter_knots(rb))
    handle = None
    for tail, head in zip(knots[:-1], knots[1:]):
        p0 = head[0], head[1]
        p1 = head[0] + cos(head[2]) * head[3],\
             head[1] + sin(head[2]) * head[3]
        p2 = tail[0] - cos(tail[2]) * tail[4],\
             tail[1] - sin(tail[2]) * tail[4]
        p3 = tail[0], tail[1]
        Ax = (
            p0[0],
            3*p1[0] - 3*p0[0],
            3*p2[0] - 6*p1[0] + 3*p0[0],
            p3[0] - 3*p2[0] + 3*p1[0] - p0[0]
        )
        Ay = (
            p0[1],
            3*p1[1] - 3*p0[1],
            3*p2[1] - 6*p1[1] + 3*p0[1],
            p3[1] - 3*p2[1] + 3*p1[1] - p0[1]
        )

        t = np.linspace(0, 1, 50)
        x = (Ax[0] + Ax[1]*t + Ax[2]*t**2 + Ax[3]*t**3)
        y = (Ay[0] + Ay[1]*t + Ay[2]*t**2 + Ay[3]*t**3)
        plt.plot(x, y, color=color)
        plt.plot([p0[0], p1[0]], [p0[1], p1[1]], color=color, linewidth=0.5)
        handle, = plt.plot([p2[0], p3[0]], [p2[1], p3[1]], color=color, linewidth=0.5)
    return handle

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

def plot_segment_bbs(segment_rb, color):
    handle = None
    read = segment_rb["read"]
    write = segment_rb["write"]
    capacity = segment_rb["capacity"]
    while read != write:
        handle = plot_aabb(segment_rb["data"][read]["bb"], color=color)
        read = (read + 1) % capacity
    return handle

color_table = (
    ("#FF8080", "#FFA0A0"),
    ("#80FF80", "#A0FFA0"),
    ("#8080FF", "#A0A0FF"),
    ("#80FFFF", "#A0FFFF"),
    ("#FF8000", "#FFA000"),
    ("#FF80FF", "#FFA0FF"),
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
                plot_knots(val["data"]["knots"], colors[0])
                plot_trails(val["data"]["trails"], colors[0])
                #plot_segment_bbs(val["data"]["segment_bbs"], colors[1])
                handle = plot_aabb(val["data"]["bb"], colors[1])

            if str(val.type).endswith("snake_data *"):
                val = val.dereference()
            if str(val.type).endswith("snake_data"):
                plot_knots(val["knots"], colors[0])
                plot_trails(val["trails"], colors[0])
                #plot_segment_bbs(val["segment_bbs"], colors[1])
                handle = plot_aabb(val["bb"], colors[1])

            if str(val.type).endswith("bezier_knot_rb *"):
                handle = plot_knots(val, colors[0])

            if str(val.type).endswith("qwpos_vec_rb *"):
                handle = plot_trails(val, colors[0])

            if str(val.type).endswith("qwpos_vec *"):
                handle = plot_head_trail(val, colors[0])

            #if str(val.type).endswith("bezier_segment_rb *"):
            #    handle = plot_segment_bbs(val, colors[1])

            if str(val.type).endswith("qwaabb"):
                handle = plot_aabb(val, colors[1])

            if handle is not None:
                handle.set_label(exprs[i])

        plt.legend()
        plt.gca().set_aspect('equal', adjustable='box')
        plt.show()

Plot()
