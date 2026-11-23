from emu import CPU, SEG2
import itertools
cpu=CPU()
base_mem=bytes(cpu.mem)
def plan(dice, cat, rollno):
    """Run original P48B5 for sorted dice -> (value, reroll flags by sorted position)."""
    cpu.mem[:]=base_mem; cpu.reset()
    d=sorted(dice)
    for i,v in enumerate(d):
        cpu.ww(SEG2+0x4F7+4*i, v); cpu.ww(SEG2+0x4F9+4*i, i+1)   # orig index = sorted pos
    cpu.ww(SEG2+0x0A, rollno)
    tbl=SEG2+0x16
    cpu.call(0x48B5, [0x0002, 0x0016, cat])
    row=tbl+(cat-1)*7
    val=cpu.rw(row); val = val-65536 if val>32767 else val
    flags=tuple(cpu.rb(row+2+i) for i in range(5))
    return val, flags
if __name__=='__main__':
    for d in [(1,2,3,4,5),(3,3,3,5,6),(2,2,5,5,6),(1,1,1,1,1),(1,3,4,5,6),(2,3,4,6,6)]:
        print(d, [plan(d,c,1) for c in range(1,14)])
    print('steps ok')
