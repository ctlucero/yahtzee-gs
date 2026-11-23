import struct,sys
d=open(__import__('os').environ.get('YAHTZEE_BIN','YAHTZEE'),'rb').read()
def segments():
    o=0
    while o<len(d)-44:
        cnt,resspc,length=struct.unpack('<III',d[o:o+12])
        kind=d[o+12]; lablen=d[o+13]; numlen=d[o+14]; ver=d[o+15]
        if cnt==0: break
        size=cnt*512 if ver==1 else cnt
        segnum,entry=struct.unpack('<HI',d[o+34:o+40]); dn,dd=struct.unpack('<HH',d[o+40:o+44])
        name=d[o+dn:o+dn+(lablen or 1+d[o+dn])]
        yield dict(off=o,size=size,len=length,res=resspc,kind=kind,ver=ver,lablen=lablen,numlen=numlen,segnum=segnum,entry=entry,name=name,body=d[o+dd:o+size])
        o+=size
if __name__=='__main__':
    for s in segments():
        print({k:v for k,v in s.items() if k!='body'}, 'body',len(s['body']))
