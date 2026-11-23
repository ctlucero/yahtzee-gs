import sys, collections
from qdemu import Toolbox
tb=Toolbox()
trace=collections.deque(maxlen=40)
orig_step=tb.step
def step():
    trace.append((tb.PBR,tb.PC,tb.S))
    orig_step()
tb.step=step
def run(addr,args=()):
    try:
        tb.call(addr,args)
    except Exception as e:
        print('ERR',repr(e))
        for t in trace: print('  %02X:%04X S=%04X'%t)
        for e in tb.log[-10:]: print(e)
        raise SystemExit
if __name__=='__main__':
    for p in sys.argv[1:]: run(int(p,16))
    for e in tb.log[:300]: print(e)

def render(path):
    tb.image().save(path)
