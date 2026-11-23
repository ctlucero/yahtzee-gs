# Bitmap font renderer used by the emulator reference renders; the C++ port
# uses the same font file so the two can be compared pixel for pixel.
class BitmapFont:
    def __init__(self, path):
        self.glyphs={}; self.ascent=7; self.descent=2
        cur=None; rows=[]
        for line in open(path, encoding='utf-8'):
            line=line.rstrip('\n')
            if line.startswith('#') and not set(line)<=set('#.'): continue
            if line.startswith('ascent'): self.ascent=int(line.split()[1]); continue
            if line.startswith('descent'): self.descent=int(line.split()[1]); continue
            if line.startswith('char '): cur=line[5]; rows=[]; continue
            if line.startswith('code '): cur=chr(int(line[5:])); rows=[]; continue
            if cur is not None and line and set(line)<=set('#.'):
                rows.append(line)
                if len(rows)==self.ascent+self.descent: self.glyphs[cur]=rows; cur=None
    def adv(self,c): g=self.glyphs.get(c, self.glyphs['?']); return len(g[0])+1

class GSFonts:
    """Fonts interface for qdemu: sizes 16/32 are 2x/4x pixel scalings."""
    def __init__(self, path='assets/fonts/gs8.txt', deco=None):
        self.f=BitmapFont(path); self.deco=deco
    def pick(self,font):
        fam,size=font
        if fam=='Art Deco' and self.deco: return self.deco,1
        return self.f, max(1,size//8)
    def metrics(self,font,face):
        f,s=self.pick(font); return (f.ascent*s, f.descent*s, 6*s, 0)
    def width(self,font,face,s):
        f,sc=self.pick(font); return sum((f.adv(c)+(1 if face&1 else 0))*sc for c in s)
    def render(self,font,face,s,x,y):
        f,sc=self.pick(font)
        for c in s:
            g=f.glyphs.get(c, f.glyphs['?'])
            ink=set()
            for r,row in enumerate(g):
                for i,ch in enumerate(row):
                    if ch=='#':
                        ink.add((i,r))
                        if face&1: ink.add((i+1,r))
            w=len(g[0])+1+(1 if face&1 else 0)
            for r in range(len(g)):
                for i in range(w):
                    on=(i,r) in ink
                    for dy in range(sc):
                        for dx in range(sc):
                            yield (x+i*sc+dx, y-f.ascent*sc+r*sc+dy, on)
            x+=w*sc
