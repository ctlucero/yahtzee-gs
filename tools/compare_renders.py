"""Renders states with the ORIGINAL code in the emulator and compares them
with the C++ port (raw dumps from tests/render_dump.cpp).
Usage: YAHTZEE_BIN=... python3 tools/compare_renders.py <dir with .raw files>"""
import sys, numpy as np, random
RAW = sys.argv[1] if len(sys.argv) > 1 else 'renders'
import runscreen as R
from fonts_gs import GSFonts
from emu import SEG2
tb=R.tb; tb.fonts=GSFonts()
def win(a): return a[15:198,3:317]
def load(name): return np.fromfile(f'{RAW}/{name}.raw',dtype=np.uint8).reshape(200,320)
def cmp(name):
    mine=load(name); emu=tb.screen
    d=win(mine)!=win(emu)
    n=int(d.sum())
    print(f'{name:14s} differing pixels in window: {n}')
    if n:
        ys,xs=np.nonzero(d); print('   bbox x',xs.min()+3,xs.max()+3,'y',ys.min()+15,ys.max()+15)
    return n
def dieval(seq):
    it=iter(seq)
    tb.t8604=lambda n,w,raw: next(it)-1
def h(addr): return tb.r32(SEG2+addr)
def P251E(ctl,hil,ref):   # call the original P251E(handle, hilite, refcon)
    R.run_args(0x251E,[ctl>>16,ctl&0xFFFF,hil,ref])
def run_args(addr,args):
    tb.call(addr,args)
R.run_args=run_args
R.run(0x8B11); R.run(0x1AB8); R.run(0x04B8); cmp('title')
R.run(0x18FD); cmp('newgame')
dieval([4,3,5,5,3]); R.run(0x15C0); cmp('roll1')
die1=h(0x145); roll=h(0x139); score=h(0x13D); chance=h(0x159+4*12)
tb.w32(tb.deref(die1)+0x20,1); tb.draw_control(die1); P251E(roll,0,1); cmp('select')
tb.ww(SEG2+0x0C,13); P251E(chance,0,1); P251E(score,0,1); cmp('category')
