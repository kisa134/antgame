# Procedural UI textures (tape, stamp frames, dot, vignette, fades, grain, scratches, torn edge). Run from the repo root.
import numpy as np, os
from PIL import Image, ImageDraw, ImageFilter
O='SourceArt/UI/Materials/'; os.makedirs(O, exist_ok=True)
rng=np.random.default_rng(7)
def save(a, n): Image.fromarray(a.astype(np.uint8), 'RGBA').save(O+'T_UI_'+n+'.png', optimize=True)
# soft dot
y,x=np.mgrid[0:64,0:64]; r=np.hypot(x-31.5,y-31.5)/32
a=np.zeros((64,64,4)); a[...,:3]=255; a[...,3]=np.clip(1-r,0,1)**1.8*255; save(a,'Dot')
# vignette
y,x=np.mgrid[0:512,0:512]; r=np.hypot((x-255.5)/256,(y-255.5)/256)
a=np.zeros((512,512,4)); a[...,3]=np.clip((r-0.45)/0.7,0,1)**1.6*255; save(a,'Vignette')
# vertical fade (top transparent -> bottom opaque)
a=np.zeros((256,8,4)); a[...,3]=np.linspace(0,255,256)[:,None]; save(a,'FadeV')
a=np.zeros((8,256,4)); a[...,3]=np.linspace(255,0,256)[None,:]; save(a,'FadeH')
# grain
n=rng.normal(128,40,(256,256)).clip(0,255)
a=np.zeros((256,256,4)); a[...,0]=a[...,1]=a[...,2]=n; a[...,3]=np.abs(n-128)*1.4; save(a,'Grain')
# scratches
im=Image.new('RGBA',(512,512),(0,0,0,0)); d=ImageDraw.Draw(im)
for i in range(14):
    x=rng.integers(0,512); w=rng.integers(1,3); al=int(rng.integers(40,140))
    y0=rng.integers(0,200); y1=rng.integers(300,512)
    d.line([(x,y0),(x+rng.integers(-6,6),y1)],fill=(255,248,230,al),width=int(w))
for i in range(40):
    x,y=rng.integers(0,512,2); s=rng.integers(1,4); d.ellipse([x,y,x+s,y+s],fill=(255,250,240,int(rng.integers(60,160))))
save(np.asarray(im),'Scratches')
# tape: beige translucent strip with jagged ends
W,H=320,72
a=np.zeros((H,W,4)); base=np.array([236,222,186])
tex=rng.normal(0,6,(H,W))
for c in range(3): a[...,c]=np.clip(base[c]+tex,0,255)
al=np.full((H,W),200.0)
for yy in range(H):
    j1=int(abs(rng.normal(0,5)))+2; j2=int(abs(rng.normal(0,5)))+2
    al[yy,:j1]=0; al[yy,W-j2:]=0
al[:2,:]*=0.5; al[-2:,:]*=0.5
a[...,3]=al; save(a,'Tape')
# stamp ring (white, tint in code)
S=256; im=Image.new('L',(S,S),0); d=ImageDraw.Draw(im)
d.ellipse([10,10,S-10,S-10],outline=255,width=14); d.ellipse([34,34,S-34,S-34],outline=255,width=5)
m=np.asarray(im).astype(float)
noise=rng.random((S,S)); m=m*(noise>0.18)
m=np.asarray(Image.fromarray(m.astype(np.uint8)).filter(ImageFilter.GaussianBlur(0.9))).astype(float)
a=np.zeros((S,S,4)); a[...,:3]=255; a[...,3]=np.clip(m*1.3,0,255); save(a,'StampRing')
# rough ink rectangle frame (for stamps like "ГОТОВ")
W,H=512,160; im=Image.new('L',(W,H),0); d=ImageDraw.Draw(im)
d.rounded_rectangle([6,6,W-6,H-6],radius=18,outline=255,width=12)
m=np.asarray(im).astype(float)*(rng.random((H,W))>0.2)
m=np.asarray(Image.fromarray(m.astype(np.uint8)).filter(ImageFilter.GaussianBlur(0.9))).astype(float)
a=np.zeros((H,W,4)); a[...,:3]=255; a[...,3]=np.clip(m*1.3,0,255); save(a,'StampBox')
# paper torn edge strip (bottom edge, white alpha), tile horizontally
W,H=512,32; a=np.zeros((H,W,4)); a[...,:3]=255
edge=(np.cumsum(rng.normal(0,1.2,W))); edge=edge-edge.mean(); edge=np.clip(16+edge*0.8+rng.normal(0,1.5,W),4,28)
for x in range(W): a[:int(edge[x]),x,3]=255
save(a,'TornEdge')
print(sorted(os.listdir(O)))
