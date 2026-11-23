import struct
from omf import segments
def load(s):
    b=s['body']; i=0; mem=bytearray(); relocs=[]
    while i<len(b):
        op=b[i]; i+=1
        if op==0: break
        elif op<=0xDF: mem+=b[i:i+op]; i+=op
        elif op==0xF2: n,=struct.unpack('<I',b[i:i+4]); i+=4; mem+=b[i:i+n]; i+=n
        elif op==0xF1: n,=struct.unpack('<I',b[i:i+4]); i+=4; mem+=bytes(n)
        elif op==0xE2: sz,sh,off,val=struct.unpack('<BbII',b[i:i+10]); i+=10; relocs.append(('R',off,sz,sh,val,None))
        elif op==0xE3: sz,sh,off,f,seg,val=struct.unpack('<BbIHHI',b[i:i+14]); i+=14; relocs.append(('I',off,sz,sh,val,seg))
        elif op==0xF5: sz,sh,off,val=struct.unpack('<BbHH',b[i:i+6]); i+=6; relocs.append(('R',off,sz,sh,val,None))
        elif op==0xF6: sz,sh,off,seg,val=struct.unpack('<BbHBH',b[i:i+7]); i+=7; relocs.append(('I',off,sz,sh,val,seg))
        elif op==0xF7:
            n,=struct.unpack('<I',b[i:i+4]); i+=4; t=b[i]; rec=b[i+1:i+n]; i+=n
            page=0; j=0
            while j<len(rec):
                c=rec[j]; j+=1
                if c&0x80: page+= (c&0x7f)+1; continue
                for k in range(c+1):
                    lo=rec[j]; j+=1; off=page*256+lo
                    relocs.append(('S',off,t,None,None,None))
                page+=1
        else: raise Exception(f'op {op:02X} at {i-1}')
    return mem,relocs
if __name__=='__main__':
    for s in segments():
        m,r=load(s); print(s['segnum'],len(m),len(r), r[:5])
