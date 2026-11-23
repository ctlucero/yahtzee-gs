"""Minimal native-mode 65816 emulator, enough to run TML Pascal procedures
from YAHTZEE in isolation and use them as a ground-truth oracle."""
import struct
from dis65 import OPS
from omf import segments
from omfrec import load

SEG1, SEG2 = 0x010000, 0x020000      # load addresses for the two segments


class Halt(Exception):
    pass


class CPU:
    def __init__(self):
        self.mem = bytearray(0x30000)
        segs = list(segments())
        code, rel1 = load(segs[0])
        data, rel2 = load(segs[1])
        self.mem[SEG1:SEG1 + len(code)] = code
        self.mem[SEG2:SEG2 + len(data)] = data
        bases = {1: SEG1, 2: SEG2}
        for kind, off, size, shift, val, seg in rel1:
            if kind == 'S':
                continue
            target = (bases[seg] if kind == 'I' else SEG1) + val
            v = target >> -shift if shift < 0 else target << shift
            for i in range(size):
                self.mem[SEG1 + off + i] = (v >> (8 * i)) & 0xFF
        self.reset()

    def reset(self):
        self.A = self.X = self.Y = 0
        self.S = 0x1FFF
        self.D = 0x0000
        self.DBR = SEG2 >> 16
        self.PBR = SEG1 >> 16
        self.PC = 0
        self.P = 0x00            # m=0, x=0: 16-bit everything
        self.steps = 0

    # --- flags -------------------------------------------------------
    def f(self, bit): return bool(self.P & bit)
    def setf(self, bit, v):
        self.P = (self.P | bit) if v else (self.P & ~bit)
    @property
    def m16(self): return not self.P & 0x20
    @property
    def x16(self): return not self.P & 0x10

    # --- memory ------------------------------------------------------
    def rb(self, a): return self.mem[a & 0xFFFFFF]
    def wb(self, a, v): self.mem[a & 0xFFFFFF] = v & 0xFF
    def rw(self, a, wide=True):
        return self.rb(a) | (self.rb(a + 1) << 8 if wide else 0)
    def ww(self, a, v, wide=True):
        self.wb(a, v)
        if wide:
            self.wb(a + 1, v >> 8)
    def push8(self, v): self.wb(self.S, v); self.S = (self.S - 1) & 0xFFFF
    def pull8(self): self.S = (self.S + 1) & 0xFFFF; return self.rb(self.S)
    def push16(self, v): self.push8(v >> 8); self.push8(v)
    def pull16(self): lo = self.pull8(); return lo | self.pull8() << 8

    def fetch(self, n):
        a = (self.PBR << 16) | self.PC
        v = 0
        for i in range(n):
            v |= self.rb((self.PBR << 16) | ((self.PC + i) & 0xFFFF)) << (8 * i)
        self.PC = (self.PC + n) & 0xFFFF
        return v

    # --- effective addresses -----------------------------------------
    def ea(self, mode, op):
        D, X, Y, DB = self.D, self.X, self.Y, self.DBR << 16
        b0 = lambda a: a & 0xFFFF
        if mode == 'd':   return b0(D + op)
        if mode == 'dx':  return b0(D + op + X)
        if mode == 'dy':  return b0(D + op + Y)
        if mode == 'di':  return DB | self.rw(b0(D + op))
        if mode == 'dxi': return DB | self.rw(b0(D + op + X))
        if mode == 'diy': return ((DB | self.rw(b0(D + op))) + Y) & 0xFFFFFF
        if mode == 'dil': p = b0(D + op); return self.rw(p) | self.rb(p + 2) << 16
        if mode == 'dily':
            p = b0(D + op); return ((self.rw(p) | self.rb(p + 2) << 16) + Y) & 0xFFFFFF
        if mode == 'sr':  return b0(self.S + op)
        if mode == 'sriy': return ((DB | self.rw(b0(self.S + op))) + Y) & 0xFFFFFF
        if mode == 'a':   return DB | op
        if mode == 'ax':  return ((DB | op) + X) & 0xFFFFFF
        if mode == 'ay':  return ((DB | op) + Y) & 0xFFFFFF
        if mode == 'al':  return op
        if mode == 'alx': return (op + X) & 0xFFFFFF
        raise NotImplementedError(mode)

    def nz(self, v, wide):
        mask = 0xFFFF if wide else 0xFF
        v &= mask
        self.setf(0x02, v == 0)
        self.setf(0x80, v & (0x8000 if wide else 0x80))
        return v

    def getA(self): return self.A if self.m16 else self.A & 0xFF
    def setA(self, v):
        if self.m16: self.A = v & 0xFFFF
        else: self.A = (self.A & 0xFF00) | (v & 0xFF)

    def adc(self, v):
        w = self.m16; mask = 0xFFFF if w else 0xFF; sign = 0x8000 if w else 0x80
        a = self.getA()
        if self.f(0x08): raise NotImplementedError('decimal mode')
        r = a + v + (1 if self.f(0x01) else 0)
        self.setf(0x01, r > mask)
        self.setf(0x40, (~(a ^ v) & (a ^ r)) & sign)
        self.setA(self.nz(r, w))

    def cmp(self, reg, v, wide):
        mask = 0xFFFF if wide else 0xFF
        r = (reg & mask) - (v & mask)
        self.setf(0x01, r >= 0)
        self.nz(r, wide)

    # --- run ---------------------------------------------------------
    def call(self, addr, args=(), sentinel=0xFFFF, max_steps=2_000_000):
        """JSL to seg1:addr with 16-bit args pushed in order; return when
        the routine RTLs back to the sentinel."""
        for a in args:
            self.push16(a)
        # fake return address (PB, PC-1) pointing at sentinel
        self.push8(0x00); self.push16(sentinel - 1)
        self.PBR, self.PC = SEG1 >> 16, addr
        while True:
            if self.PBR == 0 and self.PC == sentinel:
                return
            self.step()
            self.steps += 1
            if self.steps > max_steps:
                raise Halt('too many steps')

    def step(self):
        pc0 = self.PC
        op = self.fetch(1)
        mn, mode = OPS[op]
        if mode == '#m':  n = 2 if self.m16 else 1
        elif mode == '#x': n = 2 if self.x16 else 1
        else:
            from dis65 import SZ
            n = SZ[mode]
        operand = self.fetch(n) if n else 0
        getattr(self, 'op_' + mn, self.unimpl)(mn, mode, operand, pc0)

    def unimpl(self, mn, mode, operand, pc0):
        raise NotImplementedError(f'{mn} {mode} at {pc0:04X}')

    # operand helpers
    def load(self, mode, operand, wide):
        if mode in ('#m', '#x', '#8', 'a16'): return operand
        return self.rw(self.ea(mode, operand), wide)

    # loads/stores
    def op_LDA(self, mn, mode, o, pc): self.setA(self.nz(self.load(mode, o, self.m16), self.m16))
    def op_LDX(self, mn, mode, o, pc): self.X = self.nz(self.load(mode, o, self.x16), self.x16)
    def op_LDY(self, mn, mode, o, pc): self.Y = self.nz(self.load(mode, o, self.x16), self.x16)
    def op_STA(self, mn, mode, o, pc): self.ww(self.ea(mode, o), self.getA(), self.m16)
    def op_STX(self, mn, mode, o, pc): self.ww(self.ea(mode, o), self.X, self.x16)
    def op_STY(self, mn, mode, o, pc): self.ww(self.ea(mode, o), self.Y, self.x16)
    def op_STZ(self, mn, mode, o, pc): self.ww(self.ea(mode, o), 0, self.m16)

    # arithmetic / logic
    def op_ADC(self, mn, mode, o, pc): self.adc(self.load(mode, o, self.m16))
    def op_SBC(self, mn, mode, o, pc):
        v = self.load(mode, o, self.m16)
        self.adc(v ^ (0xFFFF if self.m16 else 0xFF))
    def op_AND(self, mn, mode, o, pc): self.setA(self.nz(self.getA() & self.load(mode, o, self.m16), self.m16))
    def op_ORA(self, mn, mode, o, pc): self.setA(self.nz(self.getA() | self.load(mode, o, self.m16), self.m16))
    def op_EOR(self, mn, mode, o, pc): self.setA(self.nz(self.getA() ^ self.load(mode, o, self.m16), self.m16))
    def op_CMP(self, mn, mode, o, pc): self.cmp(self.getA(), self.load(mode, o, self.m16), self.m16)
    def op_CPX(self, mn, mode, o, pc): self.cmp(self.X, self.load(mode, o, self.x16), self.x16)
    def op_CPY(self, mn, mode, o, pc): self.cmp(self.Y, self.load(mode, o, self.x16), self.x16)
    def op_BIT(self, mn, mode, o, pc):
        w = self.m16; v = self.load(mode, o, w)
        self.setf(0x02, (self.getA() & v) == 0)
        if mode != '#m':
            self.setf(0x80, v & (0x8000 if w else 0x80)); self.setf(0x40, v & (0x4000 if w else 0x40))

    def rmw(self, mode, o, fn):
        if mode == 'A':
            self.setA(fn(self.getA()))
        else:
            a = self.ea(mode, o); self.ww(a, fn(self.rw(a, self.m16)), self.m16)
    def op_INC(self, mn, mode, o, pc): self.rmw(mode, o, lambda v: self.nz(v + 1, self.m16))
    def op_DEC(self, mn, mode, o, pc): self.rmw(mode, o, lambda v: self.nz(v - 1, self.m16))
    def op_ASL(self, mn, mode, o, pc):
        def f(v):
            self.setf(0x01, v & (0x8000 if self.m16 else 0x80)); return self.nz(v << 1, self.m16)
        self.rmw(mode, o, f)
    def op_LSR(self, mn, mode, o, pc):
        def f(v): self.setf(0x01, v & 1); return self.nz(v >> 1, self.m16)
        self.rmw(mode, o, f)
    def op_ROL(self, mn, mode, o, pc):
        def f(v):
            c = 1 if self.f(0x01) else 0
            self.setf(0x01, v & (0x8000 if self.m16 else 0x80)); return self.nz((v << 1) | c, self.m16)
        self.rmw(mode, o, f)
    def op_ROR(self, mn, mode, o, pc):
        def f(v):
            c = (0x8000 if self.m16 else 0x80) if self.f(0x01) else 0
            self.setf(0x01, v & 1); return self.nz((v >> 1) | c, self.m16)
        self.rmw(mode, o, f)
    def op_TSB(self, mn, mode, o, pc):
        a = self.ea(mode, o); v = self.rw(a, self.m16)
        self.setf(0x02, (v & self.getA()) == 0); self.ww(a, v | self.getA(), self.m16)
    def op_TRB(self, mn, mode, o, pc):
        a = self.ea(mode, o); v = self.rw(a, self.m16)
        self.setf(0x02, (v & self.getA()) == 0); self.ww(a, v & ~self.getA(), self.m16)
    def op_INX(self, *a): self.X = self.nz(self.X + 1, self.x16)
    def op_INY(self, *a): self.Y = self.nz(self.Y + 1, self.x16)
    def op_DEX(self, *a): self.X = self.nz(self.X - 1, self.x16)
    def op_DEY(self, *a): self.Y = self.nz(self.Y - 1, self.x16)

    # transfers
    def op_TAX(self, *a): self.X = self.nz(self.A, self.x16)
    def op_TAY(self, *a): self.Y = self.nz(self.A, self.x16)
    def op_TXA(self, *a): self.setA(self.nz(self.X, self.m16))
    def op_TYA(self, *a): self.setA(self.nz(self.Y, self.m16))
    def op_TXY(self, *a): self.Y = self.nz(self.X, self.x16)
    def op_TYX(self, *a): self.X = self.nz(self.Y, self.x16)
    def op_TCD(self, *a): self.D = self.nz(self.A, True)
    def op_TDC(self, *a): self.A = self.nz(self.D, True)
    def op_TCS(self, *a): self.S = self.A & 0xFFFF
    def op_TSC(self, *a): self.A = self.nz(self.S, True)
    def op_TSX(self, *a): self.X = self.nz(self.S, self.x16)
    def op_TXS(self, *a): self.S = self.X
    def op_XBA(self, *a): self.A = ((self.A >> 8) | (self.A << 8)) & 0xFFFF; self.nz(self.A, False)

    # stack
    def op_PHA(self, *a):
        self.push16(self.A) if self.m16 else self.push8(self.A)
    def op_PLA(self, *a):
        self.setA(self.nz(self.pull16() if self.m16 else self.pull8(), self.m16))
    def op_PHX(self, *a): self.push16(self.X) if self.x16 else self.push8(self.X)
    def op_PHY(self, *a): self.push16(self.Y) if self.x16 else self.push8(self.Y)
    def op_PLX(self, *a): self.X = self.nz(self.pull16() if self.x16 else self.pull8(), self.x16)
    def op_PLY(self, *a): self.Y = self.nz(self.pull16() if self.x16 else self.pull8(), self.x16)
    def op_PHD(self, *a): self.push16(self.D)
    def op_PLD(self, *a): self.D = self.nz(self.pull16(), True)
    def op_PHB(self, *a): self.push8(self.DBR)
    def op_PLB(self, *a): self.DBR = self.nz(self.pull8(), False)
    def op_PHK(self, *a): self.push8(self.PBR)
    def op_PHP(self, *a): self.push8(self.P)
    def op_PLP(self, *a): self.P = self.pull8()
    def op_PEA(self, mn, mode, o, pc): self.push16(o)
    def op_PEI(self, mn, mode, o, pc): self.push16(self.rw((self.D + o) & 0xFFFF))
    def op_PER(self, mn, mode, o, pc): self.push16(o)

    # flags
    def op_CLC(self, *a): self.setf(0x01, False)
    def op_SEC(self, *a): self.setf(0x01, True)
    def op_CLV(self, *a): self.setf(0x40, False)
    def op_CLD(self, *a): self.setf(0x08, False)
    def op_SEI(self, *a): pass
    def op_CLI(self, *a): pass
    def op_REP(self, mn, mode, o, pc): self.P &= ~o
    def op_SEP(self, mn, mode, o, pc):
        self.P |= o
        if self.P & 0x10: self.X &= 0xFF; self.Y &= 0xFF
    def op_NOP(self, *a): pass

    # branches
    def br(self, cond, target):
        if cond: self.PC = target & 0xFFFF
    def op_BRA(self, mn, mode, o, pc): self.br(True, pc + 2 + (o - 256 if o > 127 else o))
    def op_BRL(self, mn, mode, o, pc): self.br(True, pc + 3 + (o - 65536 if o > 32767 else o))
    def _b(self, cond, o, pc): self.br(cond, pc + 2 + (o - 256 if o > 127 else o))
    def op_BEQ(self, mn, mode, o, pc): self._b(self.f(0x02), o, pc)
    def op_BNE(self, mn, mode, o, pc): self._b(not self.f(0x02), o, pc)
    def op_BCS(self, mn, mode, o, pc): self._b(self.f(0x01), o, pc)
    def op_BCC(self, mn, mode, o, pc): self._b(not self.f(0x01), o, pc)
    def op_BMI(self, mn, mode, o, pc): self._b(self.f(0x80), o, pc)
    def op_BPL(self, mn, mode, o, pc): self._b(not self.f(0x80), o, pc)
    def op_BVS(self, mn, mode, o, pc): self._b(self.f(0x40), o, pc)
    def op_BVC(self, mn, mode, o, pc): self._b(not self.f(0x40), o, pc)

    # jumps
    def op_JMP(self, mn, mode, o, pc):
        if mode == 'a': self.PC = o
        elif mode == 'ai': self.PC = self.rw(o)
        elif mode == 'axi': self.PC = self.rw((self.PBR << 16) | ((o + self.X) & 0xFFFF))
    def op_JML(self, mn, mode, o, pc):
        if mode == 'al': self.PBR, self.PC = o >> 16, o & 0xFFFF
        else:
            p = self.rw(o) | self.rb(o + 2) << 16; self.PBR, self.PC = p >> 16, p & 0xFFFF
    def op_JSR(self, mn, mode, o, pc):
        self.push16((self.PC - 1) & 0xFFFF)
        self.PC = o if mode == 'a' else self.rw((self.PBR << 16) | ((o + self.X) & 0xFFFF))
    def op_JSL(self, mn, mode, o, pc):
        if o >> 16 == 0xE1:
            raise Halt(f'toolbox/OS call at {pc:04X}')
        self.push8(self.PBR); self.push16((self.PC - 1) & 0xFFFF)
        self.PBR, self.PC = o >> 16, o & 0xFFFF
    def op_RTS(self, *a): self.PC = (self.pull16() + 1) & 0xFFFF
    def op_RTL(self, *a):
        self.PC = (self.pull16() + 1) & 0xFFFF; self.PBR = self.pull8()

def _mvn(self, mn, mode, o, pc):
    dst, src = o & 0xFF, o >> 8
    n = (self.A & 0xFFFF) + 1
    step = 1 if mn == 'MVN' else -1
    for _ in range(n):
        self.wb((dst << 16) | self.Y, self.rb((src << 16) | self.X))
        self.X = (self.X + step) & 0xFFFF; self.Y = (self.Y + step) & 0xFFFF
    self.A = 0xFFFF; self.DBR = dst
CPU.op_MVN = _mvn
CPU.op_MVP = _mvn
