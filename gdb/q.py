import math

class qw_PrettyPrinter:
    def __init__(self, qw):
        self.qw = qw

    def to_string(self):
        Q = 14
        i = int(self.qw)
        f = float(self.qw) / (1 << Q)
        return f"qw(i={i}, f={f:.4f})"


class q16_16_PrettyPrinter:
    def __init__(self, q16_16):
        self.q16_16 = q16_16

    def to_string(self):
        Q = 16
        i = int(self.q16_16)
        f = float(self.q16_16) / (1 << Q)
        return f"q16_16(i={i}, f={f:.4f})"


class qa_PrettyPrinter:
    def __init__(self, qa):
        self.qa = qa

    def to_string(self):
        Q = 12
        i = int(self.qa)
        f = float(self.qa) / (1 << Q)
        d = math.degrees(f)
        return f"qa(i={i} f={f:.4f} deg={d:.2f}°)"


class qwpos_PrettyPrinter:
    def __init__(self, qwpos):
        self.qwpos = qwpos

    def to_string(self):
        Q = 14
        x = int(self.qwpos["x"])
        y = int(self.qwpos["y"])
        x_f = float(x) / (1 << Q)
        y_f = float(y) / (1 << Q)
        return f"qwpos(i=[{x}, {y}] f=[{x_f:.4f}, {y_f:.4f}])"


class qwaabb_PrettyPrinter:
    def __init__(self, qwaabb):
        self.qwaabb = qwaabb

    def to_string(self):
        Q = 14
        x1 = int(self.qwaabb["x1"]);
        x2 = int(self.qwaabb["x2"]);
        y1 = int(self.qwaabb["y1"]);
        y2 = int(self.qwaabb["y2"]);
        x1_f = float(x1) / (1 << Q)
        x2_f = float(x2) / (1 << Q)
        y1_f = float(y1) / (1 << Q)
        y2_f = float(y2) / (1 << Q)
        return (f"qwaabb(\n"
                f"  i=[{x1}, {y1}], [{x2}, {y2}]\n"
                f"  f=[{x1_f:.4f}, {y1_f:.4f}], [{x2_f:.4f}, {y2_f:.4f}]\n)")

