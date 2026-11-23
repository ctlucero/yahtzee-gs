import re,collections
from dis65 import decode
from omf import segments; from omfrec import load
segs=list(segments()); mem,rel=load(segs[0]); data2,_=load(segs[1])
N=len(mem)
R={}
for r in rel:
    if r[0]!='S': R[r[1]]=r
TOOLSETS={1:'Tool Locator',2:'Memory Mgr',3:'Misc Tools',4:'QuickDraw II',5:'Desk Mgr',6:'Event Mgr',7:'Scheduler',8:'Sound Tools',
 9:'ADB Tools',0xA:'SANE',0xB:'Integer Math',0xC:'Text Tools',0xE:'Window Mgr',0xF:'Menu Mgr',0x10:'Control Mgr',0x11:'Loader',
 0x12:'QuickDraw Aux',0x13:'Print Mgr',0x14:'LineEdit',0x15:'Dialog Mgr',0x16:'Scrap Mgr',0x17:'Standard File',0x19:'Note Synth',
 0x1A:'Note Seq',0x1B:'Font Mgr',0x1C:'List Mgr'}
STD={1:'BootInit',2:'StartUp',3:'ShutDown',4:'Version',5:'Reset',6:'Status'}
NAMES={0x0902:'NewHandle',0x1002:'DisposeHandle',0x1B04:'SetPort',0x1C04:'GetPort',0x3A04:'MoveTo',0x3B04:'Move',0x3C04:'LineTo',
 0x3D04:'Line',0xA404:'DrawChar',0xA504:'DrawString',0x4A04:'SetRect',0x4B04:'OffsetRect',0x4C04:'InsetRect',0xA004:'SetForeColor',
 0xA204:'SetBackColor',0x1D0E:'TaskMaster',0x8604:'Random',0xA904:'StringWidth',0xA804:'CharWidth',0x3704:'SetSolidPenPat',
 0x2E04:'SetPenMode',0x2C04:'SetPenSize',0x5304:'FrameRect',0x5404:'PaintRect',0x5504:'EraseRect',0x5604:'InvertRect',
 0x5D04:'FrameRRect',0x5E04:'PaintRRect',0x5F04:'EraseRRect',0x9A04:'SetTextFace',0x9C04:'SetTextMode',0x9404:'SetFont',
 0x0D0F:'InsertMenu',0x2D0F:'NewMenu',0x2A0F:'DrawMenuBar'}
def toolname(x):
    f,t=x>>8,x&0xFF; ts=TOOLSETS.get(t,f'toolset ${t:02X}')
    n=NAMES.get(x) or (STD.get(f) and ts.replace(' ','').replace('II','')+STD[f])
    return f"{ts}: {n}" if n else f"{ts} call ${f:02X}"
# entry points
jsl_t=set(r[4] for r in rel if r[0]=='R' and r[2]==3 and mem[r[1]-1]==0x22)
pro=set(m.start()-1 for m in re.finditer(rb'\x3b\x38\xe9..\x5b',bytes(mem)) if mem[m.start()-1]==0x0b)
procs=sorted(jsl_t|pro|{0})
code={}; seen=set(); jtab={}
work=[(p,True,True) for p in procs]
while work:
    pc,m,x=work.pop()
    while 0<=pc<N and pc not in seen:
        try: mn,mode,n,v,txt=decode(mem,pc,m,x)
        except Exception: break
        if mn in('BRK','COP','STP','WDM') and pc not in procs: break
        seen.add(pc); code[pc]=(m,x)
        if mn=='REP': m=m or bool(v&0x20); x=x or bool(v&0x10)
        if mn=='SEP': m=m and not v&0x20; x=x and not v&0x10
        nxt=pc+n
        if mn=='JSL' and mem[pc+1:pc+4]==b'\xa8\x00\xe1': nxt+=6
        if mode in('r','rl'): work.append((v,m,x))
        if mn=='LDA' and mode=='alx' and pc+1 in R and mem[pc+4]==0x48 and mem[pc+5]==0x60:
            tb=R[pc+1][4]; cnt=None
            for back in range(3,16):
                if mem[pc-back]==0xC9: cnt=mem[pc-back+1]|mem[pc-back+2]<<8; break
            if cnt:
                jtab[tb]=cnt
                for k in range(cnt): work.append((R[tb+2*k][4]+1,m,x))
        if mn in('JSR','JMP') and mode=='a' and pc+1 not in R: work.append((v,m,x))
        if mn in('RTS','RTL','RTI','BRA','BRL','JML') or (mn=='JMP'): break
        pc=nxt
labels=set()
for pc in code:
    mn,mode,n,v,txt=decode(mem,pc,*code[pc])
    if mode in('r','rl') or (mn in('JSR','JMP') and mode=='a'): labels.add(v)
for tb,c in jtab.items():
    for k in range(c): labels.add(R[tb+2*k][4]+1)
def pstr(a):
    if a<N and mem[a]<200 and all(32<=c<127 or c==13 for c in mem[a+1:a+1+mem[a]]) and mem[a]>1:
        return bytes(mem[a+1:a+1+mem[a]]).decode()
out=[]
P=set(procs); idx={p:i for i,p in enumerate(procs)}
pc=0
while pc<N:
    if pc in P: 
        kind='RUNTIME LIBRARY' if pc>=0x9200 else 'PROCEDURE'
        out.append(f"\n;{'='*70}\n; {kind} P{pc:04X}\n;{'='*70}")
    if pc in code:
        mn,mode,n,v,txt=decode(mem,pc,*code[pc]); note=''
        for k in range(pc+1,pc+n):
            if k in R:
                r=R[k]
                if r[0]=='I': txt=re.sub(r'\$[0-9A-F]+',f'G{r[4]:04X}',txt,1); note='global var (data seg)'
                else:
                    tgt=r[4]; lab=('P' if tgt in P else 'L')+f'{tgt:04X}'
                    txt=re.sub(r'\$[0-9A-F]+',lab,txt,1)
                    s=pstr(tgt)
                    if s: note=f'"{s}"'
                break
        if mode in('r','rl') or (mn in('JSR','JMP') and mode=='a'):
            txt=re.sub(r'L([0-9A-F]{4})',lambda q:('P' if int(q.group(1),16) in P else 'L')+q.group(1),txt)
        if mn=='LDX' and mem[pc+n:pc+n+4]==b'\x22\x00\x00\xe1': note=toolname(v)
        if mn=='JSL' and mem[pc+1:pc+4]==b'\xa8\x00\xe1': note=f'GS/OS call ${int.from_bytes(mem[pc+4:pc+6],"little"):04X}'
        lab=f"L{pc:04X}:" if pc in labels and pc not in P else ''
        out.append(f"{lab:7s}{pc:04X}  {mem[pc:pc+n].hex():10s}  {mn} {txt}".ljust(58)+(f"; {note}" if note else ''))
        pc+=n
        if mn=='JSL' and mem[pc-3:pc]==b'\xa8\x00\xe1':
            out.append(f"       {pc:04X}  {mem[pc:pc+6].hex():10s}  DC   (GS/OS parms)"); pc+=6
    elif pc in jtab:
        for k in range(jtab[pc]):
            t=R[pc][4]+1; out.append(f"       {pc:04X}  DW   L{t:04X}-1".ljust(58)+f"; case {k}"); labels.add(t); pc+=2
    else:
        st=pc
        while pc<N and pc not in code and pc not in P and pc-st<16: pc+=1
        b=mem[st:pc]; s=pstr(st)
        asc=''.join(chr(c) if 32<=c<127 else '.' for c in b)
        out.append(f"       {st:04X}  DC H'{b.hex().upper()}'".ljust(58)+f"; {asc}")
open('YAHTZEE_disasm.s','w').write(f"""; YAHTZEE v1.1 (1988) by David W. Buell - written in TML Pascal
; Annotated 65816 disassembly of OMF segment 1 (code, ${N:X} bytes)
; Pn = procedure entry, Ln = local label, Gn = global variable in data segment 2
; Runtime library (TML Pascal support code) begins at about $9200.
; Toolbox calls are annotated as 'Toolset: Name' or 'Toolset call $nn'.
"""+'\n'.join(out)+'\n')
print('procs',len(procs),'user',sum(p<0x9200 for p in procs),'code bytes',len(seen),'of',N,'lines',len(out))
