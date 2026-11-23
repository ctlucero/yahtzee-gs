"""Toolbox emulation on top of emu.CPU: enough QuickDraw II, Memory,
Control, Window and Font Manager to run YAHTZEE's own drawing code and
capture what it draws, both as a call log and as a 320x200 picture."""
import math
import numpy as np
from emu import CPU, Halt, SEG1, SEG2

W, H = 320, 200
STD_PALETTE = [0x000, 0x777, 0x841, 0x72C, 0x00F, 0x080, 0xF70, 0xD00,
               0xFA9, 0xFF0, 0x0E0, 0x4DF, 0xDAF, 0x78F, 0xCCC, 0xFFF]

# param bytes, result bytes
SIZES = {
 0x1B04:(4,0),0x1C04:(0,4),0x2004:(4,0),0x2904:(4,0),0x2C04:(4,0),0x2D04:(4,0),0x2E04:(2,0),
 0x3204:(4,0),0x3304:(4,0),0x3404:(4,0),0x3604:(0,0),0x3704:(2,0),0x3804:(2,0),
 0x3A04:(4,0),0x3B04:(4,0),0x3C04:(4,0),0x3D04:(4,0),0x4A04:(12,0),0x4B04:(8,0),0x4C04:(8,0),
 0x5304:(4,0),0x5404:(4,0),0x5504:(4,0),0x5604:(4,0),0x5D04:(8,0),0x5E04:(8,0),0x5F04:(8,0),
 0x6704:(0,4),0x6804:(4,0),0x6904:(8,0),0x6D04:(0,0),0x6E04:(4,0),0x6F04:(8,0),0x7004:(8,0),
 0x7204:(12,0),0x7304:(12,0),0x7404:(12,0),0x7904:(4,0),0x7A04:(4,0),0x7B04:(4,0),
 0x8604:(0,2),0x8704:(4,0),0x9004:(0,0),0x9104:(0,0),0x9604:(4,0),0x9A04:(2,0),0x9C04:(2,0),
 0xA004:(2,0),0xA104:(0,2),0xA204:(2,0),0xA404:(2,0),0xA504:(4,0),0xA804:(2,2),0xA904:(4,2),
 0xCA04:(0,0),0x0E04:(6,0),0x1204:(4,0),0x1304:(2,2),
 0x0902:(12,4),0x1002:(4,0),
 0x0910:(32,4),0x1010:(4,0),0x1110:(6,0),0x1910:(6,0),0x1A10:(4,2),0x2210:(8,0),0x2310:(4,4),
 0x2510:(4,0),0x0B10:(4,0),
 0x090E:(4,4),0x130E:(4,0),0x0C0E:(6,4),
 0x0E1B:(6,0),0x0C1B:(4,2),
 0x260B:(10,0), 0x0D03:(0,8),
 0x300F:(2,0),0x310F:(2,0),0x240F:(6,0),0x2C0F:(4,0),0x2A0F:(0,0),
}


def s16(v): return v - 0x10000 if v & 0x8000 else v


class Toolbox(CPU):
    def __init__(self):
        super().__init__()
        self.mem.extend(bytes(0x100000 - len(self.mem)))
        self.heap = 0x040000
        self.handles = {}
        self.log = []
        self.screen = np.zeros((H, W), dtype=np.uint8)
        self.scb = [0] * H
        self.palettes = [list(STD_PALETTE) for _ in range(16)]
        self.regions = {}        # handle -> set of (x,y) in local coords
        self.controls = []       # handles in creation order
        self.port_origin = (3, 15)
        self.port_clip = (0, 0, 314, 183)    # left, top, right, bottom (local)
        self.pen = [0, 0]; self.pensize = (1, 1); self.penmode = 0
        self.pencolor = 0; self.fore = 15; self.back = 0; self.textmode = 0; self.textface = 0
        self.backpat = None
        self.font = ('Shaston', 8)
        self.rgn_open = None
        self.fonts = None
        self.penmask = [0xFF] * 8

    # ---- memory manager --------------------------------------------
    def alloc(self, size):
        size = max(size, 1)
        block = self.heap; self.heap += (size + 15) & ~15
        mp = self.heap; self.heap += 16
        self.ww(mp, block & 0xFFFF); self.ww(mp + 2, block >> 16)
        self.handles[mp] = block
        return mp

    def deref(self, h): return self.rw(h) | self.rw(h + 2) << 16
    def r32(self, a): return self.rw(a) | self.rw(a + 2) << 16
    def w32(self, a, v): self.ww(a, v & 0xFFFF); self.ww(a + 2, v >> 16)
    def pstr(self, a): return bytes(self.mem[a + 1:a + 1 + self.mem[a]]).decode('latin1')
    def rect(self, a): return tuple(s16(self.rw(a + 2 * i)) for i in range(4))  # top,left,bottom,right

    # ---- toolbox dispatch -------------------------------------------
    def op_JSL(self, mn, mode, o, pc):
        if o == 0xE10000:
            return self.toolcall(self.X, pc)
        if o >> 16 == 0xE1:
            raise Halt(f'OS call at {pc:04X}')
        return super().op_JSL(mn, mode, o, pc)

    def toolcall(self, num, pc):
        if num not in SIZES:
            raise Halt(f'unknown tool call ${num:04X} at {pc:04X}')
        psize, rsize = SIZES[num]
        raw = bytes(self.pull8() for _ in range(psize))
        # params as words from top of stack (last pushed first)
        words = [raw[i] | raw[i + 1] << 8 for i in range(0, psize - 1, 2)]
        result_addr = (self.S + 1) & 0xFFFF
        res = getattr(self, f't{num:04X}', self.tstub)(num, words, raw) or 0
        for i in range(rsize):
            self.wb(result_addr + i, res >> (8 * i))
        self.A = 0; self.setf(0x01, False)

    def tstub(self, num, w, raw):
        self.log.append(('stub', f'{num:04X}', w))
        return 0

    @staticmethod
    def L(w, i):            # long from word list (low word first in memory order)
        return w[i] | w[i + 1] << 16

    # ---- nested calls back into emulated code -------------------------
    def callback(self, addr24, words_in_push_order, result_bytes=0):
        saved = (self.A, self.X, self.Y, self.D, self.DBR, self.PBR, self.PC, self.P)
        for _ in range(result_bytes // 2):
            self.push16(0)
        for wv in words_in_push_order:
            self.push16(wv)
        self.push8(0); self.push16(0xFFF0 - 1)
        self.PBR, self.PC = addr24 >> 16, addr24 & 0xFFFF
        self.D = 0; self.P = 0
        while not (self.PBR == 0 and self.PC == 0xFFF0):
            self.step()
        res = 0
        for i in range(result_bytes // 2):
            res |= self.pull16() << (16 * i)
        self.A, self.X, self.Y, self.D, self.DBR, self.PBR, self.PC, self.P = saved
        return res

    # ---- drawing primitives -----------------------------------------
    def plot(self, x, y, color):
        l, t, r, b = self.port_clip
        if not (l <= x < r and t <= y < b):
            return
        gx, gy = x + self.port_origin[0], y + self.port_origin[1]
        if 0 <= gx < W and 0 <= gy < H:
            self.screen[gy, gx] = color & 15

    def combine(self, x, y, src, mode):
        l, t, r, b = self.port_clip
        gx, gy = x + self.port_origin[0], y + self.port_origin[1]
        if not (l <= x < r and t <= y < b and 0 <= gx < W and 0 <= gy < H):
            return
        dst = int(self.screen[gy, gx])
        m = mode & 7
        if m == 0: v = src
        elif m == 1: v = dst | src
        elif m == 2: v = dst ^ src
        elif m == 3: v = dst & ~src
        elif m == 4: v = ~src
        elif m == 5: v = dst | ~src
        elif m == 6: v = dst ^ ~src
        else: v = dst & src
        self.screen[gy, gx] = v & 15

    def fill_pixels(self, pts, color, mode=0):
        for (x, y) in pts:
            self.combine(x, y, color, mode)

    def pen_pixels(self, pts):
        if self.rgn_open is not None:
            return
        ox, oy = self.port_origin
        for (x, y) in pts:
            gx, gy = x + ox, y + oy
            if self.penmask[gy % 8] & (0x80 >> (gx % 8)):
                self.combine(x, y, self.pencolor, self.penmode)

    @staticmethod
    def rect_pts(t, l, b, r):
        return {(x, y) for y in range(t, b) for x in range(l, r)}

    @staticmethod
    def rrect_pts(t, l, b, r, ow, oh):
        pts = set()
        a, bb = ow / 2.0, oh / 2.0
        hgt = b - t
        for y in range(t, b):
            yy = y - t
            inset = 0
            if bb > 0:
                dy = None
                if yy < bb: dy = bb - yy - 0.5
                elif yy >= hgt - bb: dy = yy - (hgt - bb) + 0.5
                if dy is not None:
                    f = 1 - (dy / bb) ** 2
                    inset = int(round(a - a * math.sqrt(max(f, 0))))
            for x in range(l + inset, r - inset):
                pts.add((x, y))
        return pts

    def line_pts(self, x0, y0, x1, y1):
        pw, ph = self.pensize
        pts = set()
        dx, dy = abs(x1 - x0), abs(y1 - y0)
        sx = 1 if x1 >= x0 else -1; sy = 1 if y1 >= y0 else -1
        err = dx - dy; x, y = x0, y0
        while True:
            for j in range(ph):
                for i in range(pw):
                    pts.add((x + i, y + j))
            if x == x1 and y == y1: break
            e2 = 2 * err
            if e2 > -dy: err -= dy; x += sx
            if e2 < dx: err += dx; y += sy
        return pts

    # ---- QuickDraw II -------------------------------------------------
    def t1B04(self, n, w, raw): self.log.append(('SetPort',))
    def t1C04(self, n, w, raw): return 0x00ABCDEF
    def t2004(self, n, w, raw):
        a = self.L(w, 0); l, t, r, b = self.port_clip
        for i, v in enumerate((t, l, b, r)): self.ww(a + 2 * i, v)
    def t2904(self, n, w, raw):
        a = self.L(w, 0); self.ww(a, self.pen[1]); self.ww(a + 2, self.pen[0])
    def t2C04(self, n, w, raw): self.pensize = (w[1], w[0]); self.log.append(('PenSize', w[1], w[0]))
    def t2D04(self, n, w, raw):
        a = self.L(w, 0); self.ww(a, self.pensize[1]); self.ww(a + 2, self.pensize[0])
    def t2E04(self, n, w, raw): self.penmode = w[0]; self.log.append(('PenMode', w[0]))
    def t3404(self, n, w, raw):
        a = self.L(w, 0)
        self.backpat = [[(self.rb(a + r * 4 + c // 2) >> (4 if c % 2 == 0 else 0)) & 15 for c in range(8)] for r in range(8)]
        self.log.append(('BackPat', self.backpat))
    def t3204(self, n, w, raw):
        a = self.L(w, 0); self.penmask = [self.rb(a + i) for i in range(8)]
        self.log.append(('PenMask', ['%02X' % m for m in self.penmask]))
    def t3304(self, n, w, raw):
        a = self.L(w, 0)
        for i in range(8): self.wb(a + i, self.penmask[i])
    def t3704(self, n, w, raw): self.pencolor = w[0]; self.log.append(('PenColor', w[0]))
    def t3A04(self, n, w, raw): self.pen = [s16(w[1]), s16(w[0])]; self.rgn_point(None)
    def t3B04(self, n, w, raw): self.pen = [self.pen[0] + s16(w[1]), self.pen[1] + s16(w[0])]; self.rgn_point(None)
    def t3C04(self, n, w, raw): self.lineto(s16(w[1]), s16(w[0]))
    def t3D04(self, n, w, raw): self.lineto(self.pen[0] + s16(w[1]), self.pen[1] + s16(w[0]))

    def lineto(self, x, y):
        x0, y0 = self.pen
        if self.rgn_open is not None:
            self.rgn_open['poly'][-1].append((x, y))
        else:
            self.log.append(('Line', x0, y0, x, y, self.pensize, self.pencolor))
            self.pen_pixels(self.line_pts(x0, y0, x, y))
        self.pen = [x, y]

    def rgn_point(self, _):
        if self.rgn_open is not None:
            self.rgn_open['poly'].append([tuple(self.pen)])

    def t4A04(self, n, w, raw):
        a = self.L(w, 4)   # rectPtr pushed first -> deepest -> w[4..5]
        l, t, r, b = s16(w[3]), s16(w[2]), s16(w[1]), s16(w[0])
        for i, v in enumerate((t, l, b, r)): self.ww(a + 2 * i, v)
    def t4B04(self, n, w, raw):
        a = self.L(w, 2); dh, dv = s16(w[1]), s16(w[0]); t, l, b, r = self.rect(a)
        for i, v in enumerate((t + dv, l + dh, b + dv, r + dh)): self.ww(a + 2 * i, v)
    def t4C04(self, n, w, raw):
        a = self.L(w, 2); dh, dv = s16(w[1]), s16(w[0]); t, l, b, r = self.rect(a)
        for i, v in enumerate((t + dv, l + dh, b - dv, r - dh)): self.ww(a + 2 * i, v)

    def shape(self, kind, rect, ow=0, oh=0):
        t, l, b, r = rect
        return self.rect_pts(t, l, b, r) if kind == 'rect' else self.rrect_pts(t, l, b, r, ow, oh)

    def frame(self, kind, rect, ow=0, oh=0):
        t, l, b, r = rect; pw, ph = self.pensize
        outer = self.shape(kind, rect, ow, oh)
        inner = self.shape(kind, (t + ph, l + pw, b - ph, r - pw), max(ow - 2 * pw, 0), max(oh - 2 * ph, 0))
        return outer - inner

    def erase_pts(self, pts):
        for (x, y) in pts:
            gx, gy = x + self.port_origin[0], y + self.port_origin[1]
            c = self.backpat[gy % 8][gx % 8] if self.backpat else self.back
            self.combine(x, y, c, 0)

    def t5304(self, n, w, raw):
        r = self.rect(self.L(w, 0)); self.log.append(('FrameRect', r, self.pencolor))
        if self.rgn_open is not None: self.rgn_open['shapes'].append(self.shape('rect', r)); return
        self.pen_pixels(self.frame('rect', r))
    def t5404(self, n, w, raw):
        r = self.rect(self.L(w, 0)); self.log.append(('PaintRect', r, self.pencolor)); self.pen_pixels(self.shape('rect', r))
    def t5504(self, n, w, raw):
        r = self.rect(self.L(w, 0)); self.log.append(('EraseRect', r)); self.erase_pts(self.shape('rect', r))
    def t5604(self, n, w, raw):
        r = self.rect(self.L(w, 0)); self.log.append(('InvertRect', r)); self.fill_pixels(self.shape('rect', r), 15, 2)
    def t5D04(self, n, w, raw):
        r = self.rect(self.L(w, 2)); ow, oh = w[1], w[0]
        self.log.append(('FrameRRect', r, ow, oh, self.pensize, self.pencolor))
        if self.rgn_open is not None: self.rgn_open['shapes'].append(self.shape('rr', r, ow, oh)); return
        self.pen_pixels(self.frame('rr', r, ow, oh))
    def t5E04(self, n, w, raw):
        r = self.rect(self.L(w, 2)); ow, oh = w[1], w[0]
        self.log.append(('PaintRRect', r, ow, oh, self.pencolor)); self.pen_pixels(self.shape('rr', r, ow, oh))
    def t5F04(self, n, w, raw):
        r = self.rect(self.L(w, 2)); ow, oh = w[1], w[0]; self.erase_pts(self.shape('rr', r, ow, oh))

    # regions
    def newrgn(self):
        h = self.alloc(16); self.regions[h] = set(); return h
    def t6704(self, n, w, raw): return self.newrgn()
    def t6804(self, n, w, raw): self.regions.pop(self.L(w, 0), None)
    def t6904(self, n, w, raw): self.regions[self.L(w, 0)] = set(self.regions.get(self.L(w, 2), set()))
    def t6D04(self, n, w, raw): self.rgn_open = {'poly': [[tuple(self.pen)]], 'shapes': []}
    def t6E04(self, n, w, raw):
        ro = self.rgn_open; self.rgn_open = None
        pts = set()
        polys = [p for p in ro['poly'] if len(p) > 2]
        if polys:
            xs = [x for p in polys for x, _ in p]; ys = [y for p in polys for _, y in p]
            for y in range(min(ys), max(ys)):
                for x in range(min(xs), max(xs)):
                    cx, cy = x + 0.5, y + 0.5; inside = False
                    for p in polys:
                        for (x1, y1), (x2, y2) in zip(p, p[1:]):
                            if (y1 > cy) != (y2 > cy):
                                xi = x1 + (cy - y1) * (x2 - x1) / (y2 - y1)
                                if xi > cx: inside = not inside
                    if inside: pts.add((x, y))
        for s in ro['shapes']:
            pts ^= s
        self.regions[self.L(w, 0)] = pts
        self.log.append(('CloseRgn', self.L(w, 0), len(pts)))
    def t6F04(self, n, w, raw):
        h = self.L(w, 2); dh, dv = s16(w[1]), s16(w[0])
        self.regions[h] = {(x + dh, y + dv) for x, y in self.regions.get(h, set())}
    def t7004(self, n, w, raw):
        pass
    def rgn3(self, w, op):
        dst, b, a = self.L(w, 0), self.L(w, 2), self.L(w, 4)
        self.regions[dst] = op(self.regions.get(a, set()), self.regions.get(b, set()))
    def t7204(self, n, w, raw): self.rgn3(w, lambda a, b: a | b)
    def t7304(self, n, w, raw): self.rgn3(w, lambda a, b: a - b)
    def t7404(self, n, w, raw): self.rgn3(w, lambda a, b: a ^ b)
    def t7A04(self, n, w, raw):
        h = self.L(w, 0); self.log.append(('PaintRgn', h, self.pencolor, len(self.regions.get(h, ()))))
        self.pen_pixels(self.regions.get(h, set()))

    def t8604(self, n, w, raw):
        import random; return random.randint(0, 0xFFFF)
    def t9604(self, n, w, raw):
        a = self.L(w, 0); asc, desc, wid, lead = self.font_metrics()
        for i, v in enumerate((asc, desc, wid, lead)): self.ww(a + 2 * i, v)
    def t9A04(self, n, w, raw): self.textface = w[0]; self.log.append(('TextFace', w[0]))
    def t9C04(self, n, w, raw): self.textmode = w[0]; self.log.append(('TextMode', w[0]))
    def tA004(self, n, w, raw): self.fore = w[0]; self.log.append(('ForeColor', w[0]))
    def tA104(self, n, w, raw): return self.fore
    def tA204(self, n, w, raw): self.back = w[0]; self.log.append(('BackColor', w[0]))
    def tA404(self, n, w, raw): self.draw_text(chr(w[0] & 0xFF))
    def tA504(self, n, w, raw): self.draw_text(self.pstr(self.L(w, 0)))
    def tA804(self, n, w, raw): return self.text_width(chr(w[0] & 0xFF))
    def tA904(self, n, w, raw): return self.text_width(self.pstr(self.L(w, 0)))
    def t0E04(self, n, w, raw):
        a = self.L(w, 0); tbl = w[2]
        self.palettes[tbl] = [self.rw(a + 2 * i) for i in range(16)]
        self.log.append(('SetColorTable', tbl, ['%03X' % c for c in self.palettes[tbl]]))
    def t1204(self, n, w, raw): self.scb[w[1]] = w[0] & 0xFF
    def t1304(self, n, w, raw): return self.scb[w[0]]

    # ---- text (pluggable font renderer) -------------------------------
    def font_metrics(self):
        return self.fonts.metrics(self.font, self.textface) if self.fonts else (8, 2, 8, 0)
    def text_width(self, s):
        return self.fonts.width(self.font, self.textface, s) if self.fonts else 8 * len(s)
    def draw_text(self, s):
        x, y = self.pen
        self.log.append(('Text', s, x, y, self.font, self.textface, self.fore, self.back, self.textmode))
        if self.fonts:
            for (px, py, on) in self.fonts.render(self.font, self.textface, s, x, y):
                m = self.textmode & 7
                if on:
                    self.combine(px, py, self.fore, m & 3)
                elif m < 4:
                    self.combine(px, py, self.back, m & 3)
        self.pen[0] += self.text_width(s)

    # ---- memory manager -----------------------------------------------
    def t0902(self, n, w, raw):
        size = self.L(w, 4); return self.alloc(size)
    def t1002(self, n, w, raw): pass

    # ---- window / font manager ---------------------------------------
    def t090E(self, n, w, raw):
        self.log.append(('NewWindow',)); self.font = ('Shaston', 8); return 0x00ABCDEF
    def t130E(self, n, w, raw): pass
    def t0E1B(self, n, w, raw):
        fam, style, size = w[1], w[2] & 0xFF, w[2] >> 8
        names = {0xFFFE: 'Shaston', 0: 'Shaston', 0x1000: 'Art Deco'}
        self.font = (names.get(fam, f'fam{fam:04X}'), size)
        self.log.append(('InstallFont', hex(fam), style, size))
    FAMILIES = {'Shaston': 0xFFFE, 'Art Deco': 0x1000}
    def t0C1B(self, n, w, raw):
        name = self.pstr(self.L(w, 0)); self.log.append(('FamNum', name))
        return self.FAMILIES.get(name, 0x2000)
    def t260B(self, n, w, raw):
        signed, length, ptr, value = w[0], w[1], self.L(w, 2), w[4]
        v = s16(value) if signed else value
        s = str(v).rjust(length)[:length]
        for i, ch in enumerate(s): self.wb(ptr + i, ord(ch))

    # ---- control manager ----------------------------------------------
    def t0910(self, n, w, raw):
        # pushed: window(4) rect(4) title(4) flag value p1 p2 defproc(4) refcon(4) color(4)
        color, refcon, defproc = self.L(w, 0), self.L(w, 2), self.L(w, 4)
        p2, p1, value, flag = w[6], w[7], w[8], w[9]
        title, rectp = self.L(w, 10), self.L(w, 12)
        h = self.alloc(48); rec = self.deref(h)
        for i, v in enumerate(self.rect(rectp)): self.ww(rec + 8 + 2 * i, v)
        self.wb(rec + 0x10, flag); self.wb(rec + 0x11, flag >> 8)
        self.ww(rec + 0x12, value); self.w32(rec + 0x14, defproc)
        self.w32(rec + 0x1C, (p2 << 16) | p1); self.w32(rec + 0x20, refcon); self.w32(rec + 0x24, color)
        self.w32(rec + 0x28, title)
        self.controls.append(h)
        self.log.append(('NewControl', hex(h), self.rect(rectp), hex(defproc), value, flag))
        self.callback(defproc, [3, 0, 0, h >> 16, h & 0xFFFF], 4)   # initCtl
        return h
    def draw_control(self, h):
        rec = self.deref(h)
        if self.rb(rec + 0x10) & 0x80: return       # invisible
        defproc = self.r32(rec + 0x14)
        self.callback(defproc, [0, 0, 0, h >> 16, h & 0xFFFF], 4)
    def t1010(self, n, w, raw):
        for h in self.controls: self.draw_control(h)
    def t1110(self, n, w, raw):
        h = self.L(w, 0); self.wb(self.deref(h) + 0x11, w[2]); self.draw_control(h)
    def t1910(self, n, w, raw):
        h = self.L(w, 0); self.ww(self.deref(h) + 0x12, w[2]); self.draw_control(h)
    def t1A10(self, n, w, raw): return self.rw(self.deref(self.L(w, 0)) + 0x12)
    def t2210(self, n, w, raw):    # SetCtlRefCon(refCon, handle)
        h = self.L(w, 0); self.w32(self.deref(h) + 0x20, self.L(w, 2))
    def t2510(self, n, w, raw): self.draw_control(self.L(w, 0))
    def t2310(self, n, w, raw): return self.r32(self.deref(self.L(w, 0)) + 0x20)   # GetCtlRefCon

    # ---- output -------------------------------------------------------
    def image(self, scale=3):
        from PIL import Image
        rgb = np.zeros((H, W, 3), dtype=np.uint8)
        for y in range(H):
            pal = self.palettes[self.scb[y] & 15]
            for x in range(W):
                c = pal[self.screen[y, x]]
                rgb[y, x] = (((c >> 8) & 15) * 17, ((c >> 4) & 15) * 17, (c & 15) * 17)
        img = Image.fromarray(rgb, 'RGB')
        return img.resize((W * scale, H * scale * 6 // 5), Image.NEAREST)
