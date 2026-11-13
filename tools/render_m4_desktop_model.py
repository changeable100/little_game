"""Render the actual exported mesh offline, without browser or UI automation.

PNG previews use the viewer's perspective, depth test and face lighting.
They are mesh renders, not browser screenshots or physics evidence.
"""
from pathlib import Path
import json,math,struct,zlib
import numpy as np
R=Path(__file__).resolve().parent.parent/'models/level2-desktop'
m=json.loads((R/'model.json').read_text(encoding='utf-8'))
def unit(v):return v/np.linalg.norm(v)
def png(path,pixels):
 h,w,_=pixels.shape
 def chunk(t,d):return struct.pack('>I',len(d))+t+d+struct.pack('>I',zlib.crc32(t+d)&0xffffffff)
 data=b''.join(b'\0'+row.tobytes() for row in pixels)
 path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(data))+chunk(b'IEND',b''))
def render(name,target,yaw,pitch,dist):
 w,h=1800,1400
 color=np.empty((h,w,3),dtype=np.float32);color[:]=[19,36,48]
 depth=np.full((h,w),np.inf)
 target=np.array(target);eye=target+dist*np.array([math.cos(yaw)*math.cos(pitch),math.sin(yaw)*math.cos(pitch),math.sin(pitch)])
 forward=unit(target-eye);right=unit(np.cross(forward,[0,0,1]));up=np.cross(right,forward)
 f=h/2/math.tan(math.pi/7);light=unit(np.array([.35,-.45,1.]))
 def project(p):
  delta=p-eye;z=delta@forward
  return np.stack([w/2+(delta@right)*f/z,h/2-(delta@up)*f/z,z],axis=-1)
 for i in range(-8,9):
  for a,b in [([-8,i,-2.025],[8,i,-2.025]),([i,-8,-2.025],[i,8,-2.025])]:
   points=project(np.array([a,b]));steps=int(max(abs(points[1,:2]-points[0,:2])))+1
   if steps>10000:continue
   line=np.linspace(points[0,:2],points[1,:2],steps).astype(int)
   good=(line[:,0]>=0)&(line[:,0]<w)&(line[:,1]>=0)&(line[:,1]<h)
   line=line[good];color[line[:,1],line[:,0]]=[42,61,72]
 for o in m['objects']:
  pos=np.asarray(o['p']).reshape(-1,3,3);normal=np.asarray(o['n']).reshape(-1,3,3);rgb=np.asarray(o['c']).reshape(-1,3,3)
  screen=project(pos)
  for p,n,c in zip(screen,normal,rgb):
   if min(p[:,2])<=.08:continue
   x0=max(0,int(np.floor(min(p[:,0]))));x1=min(w-1,int(np.ceil(max(p[:,0]))))
   y0=max(0,int(np.floor(min(p[:,1]))));y1=min(h-1,int(np.ceil(max(p[:,1]))))
   if x1<x0 or y1<y0:continue
   a,b,d=p;den=(b[1]-d[1])*(a[0]-d[0])+(d[0]-b[0])*(a[1]-d[1])
   if abs(den)<1e-8:continue
   yy,xx=np.mgrid[y0:y1+1,x0:x1+1];xx=xx+.5;yy=yy+.5
   u=((b[1]-d[1])*(xx-d[0])+(d[0]-b[0])*(yy-d[1]))/den
   v=((d[1]-a[1])*(xx-d[0])+(a[0]-d[0])*(yy-d[1]))/den;t=1-u-v
   mask=(u>=-1e-7)&(v>=-1e-7)&(t>=-1e-7)
   denom=u/a[2]+v/b[2]+t/d[2]
   z=np.divide(1,denom,out=np.full_like(denom,np.inf),where=denom>0)
   region=depth[y0:y1+1,x0:x1+1];mask&=z<region
   if not mask.any():continue
   region[mask]=z[mask]
   color[y0:y1+1,x0:x1+1][mask]=c[0]*255*(.52+.48*abs(unit(n[0])@light))
 pixels=color.reshape(h//2,2,w//2,2,3).mean(axis=(1,3)).clip(0,255).astype(np.uint8)
 path=R/name;png(path,pixels);print(path)
render('bend-render.png',[-.15,1.85,4.05],-2.4,.8,7)
render('platform-render.png',[-4,.5,2.1],-2.4,.85,10)
render('whole-render.png',[-.4,1.2,1.8],-2.4,.8,20)
