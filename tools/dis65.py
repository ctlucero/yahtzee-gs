# Minimal 65816 disassembler with M/X tracking and OMF relocation awareness
import struct
OPS={}
def add(code,mn,mode): OPS[code]=(mn,mode)
tbl="""00 BRK s8;01 ORA dxi;02 COP s8;03 ORA sr;04 TSB d;05 ORA d;06 ASL d;07 ORA dil;08 PHP i;09 ORA #m;0A ASL A;0B PHD i;0C TSB a;0D ORA a;0E ASL a;0F ORA al
10 BPL r;11 ORA diy;12 ORA di;13 ORA sriy;14 TRB d;15 ORA dx;16 ASL dx;17 ORA dily;18 CLC i;19 ORA ay;1A INC A;1B TCS i;1C TRB a;1D ORA ax;1E ASL ax;1F ORA alx
20 JSR a;21 AND dxi;22 JSL al;23 AND sr;24 BIT d;25 AND d;26 ROL d;27 AND dil;28 PLP i;29 AND #m;2A ROL A;2B PLD i;2C BIT a;2D AND a;2E ROL a;2F AND al
30 BMI r;31 AND diy;32 AND di;33 AND sriy;34 BIT dx;35 AND dx;36 ROL dx;37 AND dily;38 SEC i;39 AND ay;3A DEC A;3B TSC i;3C BIT ax;3D AND ax;3E ROL ax;3F AND alx
40 RTI i;41 EOR dxi;42 WDM s8;43 EOR sr;44 MVP mv;45 EOR d;46 LSR d;47 EOR dil;48 PHA i;49 EOR #m;4A LSR A;4B PHK i;4C JMP a;4D EOR a;4E LSR a;4F EOR al
50 BVC r;51 EOR diy;52 EOR di;53 EOR sriy;54 MVN mv;55 EOR dx;56 LSR dx;57 EOR dily;58 CLI i;59 EOR ay;5A PHY i;5B TCD i;5C JML al;5D EOR ax;5E LSR ax;5F EOR alx
60 RTS i;61 ADC dxi;62 PER rl;63 ADC sr;64 STZ d;65 ADC d;66 ROR d;67 ADC dil;68 PLA i;69 ADC #m;6A ROR A;6B RTL i;6C JMP ai;6D ADC a;6E ROR a;6F ADC al
70 BVS r;71 ADC diy;72 ADC di;73 ADC sriy;74 STZ dx;75 ADC dx;76 ROR dx;77 ADC dily;78 SEI i;79 ADC ay;7A PLY i;7B TDC i;7C JMP axi;7D ADC ax;7E ROR ax;7F ADC alx
80 BRA r;81 STA dxi;82 BRL rl;83 STA sr;84 STY d;85 STA d;86 STX d;87 STA dil;88 DEY i;89 BIT #m;8A TXA i;8B PHB i;8C STY a;8D STA a;8E STX a;8F STA al
90 BCC r;91 STA diy;92 STA di;93 STA sriy;94 STY dx;95 STA dx;96 STX dy;97 STA dily;98 TYA i;99 STA ay;9A TXS i;9B TXY i;9C STZ a;9D STA ax;9E STZ ax;9F STA alx
A0 LDY #x;A1 LDA dxi;A2 LDX #x;A3 LDA sr;A4 LDY d;A5 LDA d;A6 LDX d;A7 LDA dil;A8 TAY i;A9 LDA #m;AA TAX i;AB PLB i;AC LDY a;AD LDA a;AE LDX a;AF LDA al
B0 BCS r;B1 LDA diy;B2 LDA di;B3 LDA sriy;B4 LDY dx;B5 LDA dx;B6 LDX dy;B7 LDA dily;B8 CLV i;B9 LDA ay;BA TSX i;BB TYX i;BC LDY ax;BD LDA ax;BE LDX ay;BF LDA alx
C0 CPY #x;C1 CMP dxi;C2 REP #8;C3 CMP sr;C4 CPY d;C5 CMP d;C6 DEC d;C7 CMP dil;C8 INY i;C9 CMP #m;CA DEX i;CB WAI i;CC CPY a;CD CMP a;CE DEC a;CF CMP al
D0 BNE r;D1 CMP diy;D2 CMP di;D3 CMP sriy;D4 PEI d;D5 CMP dx;D6 DEC dx;D7 CMP dily;D8 CLD i;D9 CMP ay;DA PHX i;DB STP i;DC JML ail;DD CMP ax;DE DEC ax;DF CMP alx
E0 CPX #x;E1 SBC dxi;E2 SEP #8;E3 SBC sr;E4 CPX d;E5 SBC d;E6 INC d;E7 SBC dil;E8 INX i;E9 SBC #m;EA NOP i;EB XBA i;EC CPX a;ED SBC a;EE INC a;EF SBC al
F0 BEQ r;F1 SBC diy;F2 SBC di;F3 SBC sriy;F4 PEA a16;F5 SBC dx;F6 INC dx;F7 SBC dily;F8 SED i;F9 SBC ay;FA PLX i;FB XCE i;FC JSR axi;FD SBC ax;FE INC ax;FF SBC alx"""
for line in tbl.split('\n'):
    for it in line.split(';'):
        c,mn,mode=it.split(); add(int(c,16),mn,mode)
SZ={'i':0,'A':0,'s8':1,'d':1,'dx':1,'dy':1,'di':1,'dxi':1,'diy':1,'dil':1,'dily':1,'sr':1,'sriy':1,'r':1,'#8':1,
    'a':2,'ax':2,'ay':2,'ai':2,'axi':2,'rl':2,'mv':2,'a16':2,'ail':2,'al':3,'alx':3}
FMT={'i':'','A':'','s8':'${:02X}','d':'${:02X}','dx':'${:02X},X','dy':'${:02X},Y','di':'(${:02X})','dxi':'(${:02X},X)',
 'diy':'(${:02X}),Y','dil':'[${:02X}]','dily':'[${:02X}],Y','sr':'${:02X},S','sriy':'(${:02X},S),Y','#8':'#${:02X}',
 'a':'${:04X}','ax':'${:04X},X','ay':'${:04X},Y','ai':'(${:04X})','axi':'(${:04X},X)','a16':'#${:04X}','ail':'[${:04X}]',
 'al':'${:06X}','alx':'${:06X},X'}
def decode(mem,pc,m16,x16):
    op=mem[pc]; mn,mode=OPS[op]
    if mode=='#m': n=2 if m16 else 1
    elif mode=='#x': n=2 if x16 else 1
    else: n=SZ[mode]
    raw=mem[pc+1:pc+1+n]; v=int.from_bytes(raw,'little') if n else 0
    if mode in('#m','#x'): txt=('#${:0%dX}'%(2*n)).format(v)
    elif mode=='r': t=pc+2+(v-256 if v>127 else v); txt='L{:04X}'.format(t); v=t
    elif mode=='rl': t=(pc+3+(v-65536 if v>32767 else v))&0xFFFF; txt='L{:04X}'.format(t); v=t
    elif mode=='mv': txt='${:02X},${:02X}'.format(raw[1],raw[0])
    else: txt=FMT[mode].format(v) if FMT[mode] else ''
    return mn,mode,1+n,v,txt
