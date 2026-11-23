"""List files on a ProDOS disk image.
Usage: PRODOS_IMAGE=path/to/disk.hdv python3 tools/prodos2.py [/DIR/PATH]"""
import struct,sys,os
img=open(os.environ.get('PRODOS_IMAGE','disk.hdv'),'rb').read()
blk=lambda n: img[n*512:(n+1)*512]
def entries(key):
    b=key; first=True
    while b:
        data=blk(b); nxt=struct.unpack('<H',data[2:4])[0]
        for i in range(13):
            e=data[4+i*39:4+(i+1)*39]
            if first and i==0: first=False; continue
            st=e[0]>>4; nl=e[0]&15
            if st==0: continue
            yield dict(name=e[1:1+nl].decode(),st=st,ft=e[16],kp=struct.unpack('<H',e[17:19])[0],
                 eof=e[21]|e[22]<<8|e[23]<<16,aux=struct.unpack('<H',e[31:33])[0],
                 hdr=struct.unpack('<H',e[37:39])[0])
        b=nxt
def read(st,kp,eof):
    if st==1: return blk(kp)[:eof]
    if st==2:
        ib=blk(kp); out=b''
        for i in range(256):
            n=ib[i]|ib[256+i]<<8
            out+= blk(n) if n else b'\0'*512
            if len(out)>=eof: break
        return out[:eof]
    if st==3:
        mb=blk(kp); out=b''
        for j in range(128):
            m=mb[j]|mb[256+j]<<8
            if len(out)>=eof: break
            out+= read(2,m,256*512) if m else b'\0'*256*512
        return out[:eof]
    raise Exception(st)
def fork(e):
    """returns (data, rsrc)"""
    if e['st']==5:
        x=blk(e['kp'])
        def sub(o):
            st=x[o]; kp=struct.unpack('<H',x[o+1:o+3])[0]; eof=x[o+5]|x[o+6]<<8|x[o+7]<<16
            return read(st,kp,eof) if eof else b''
        return sub(0),sub(256)
    return read(e['st'],e['kp'],e['eof']),None
def find(path):
    key=2; e=None
    for part in path.strip('/').split('/'):
        e=next(x for x in entries(key) if x['name'].lower()==part.lower())
        key=e['kp']
    return e
def ls(path):
    e=find(path) if path else {'kp':2}
    for x in entries(e['kp']):
        print(f"{x['name']:20s} type=${x['ft']:02X} aux=${x['aux']:04X} st={x['st']:X} eof={x['eof']}")
if __name__=='__main__':
    ls(sys.argv[1] if len(sys.argv)>1 else '')
