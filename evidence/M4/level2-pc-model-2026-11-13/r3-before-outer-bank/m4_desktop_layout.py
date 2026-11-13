"""Desktop r3 layout. Deliberately independent of the unrevised board map."""
from pathlib import Path
import math,re

def build(obj,prism,quad,tri):
 blue=0x66b9d6;dark=0x32768c;green=0x8ac879
 route=[];bends=[];checkpoints=[]
 def rect(x0,x1,y0,y1):return [[x0,y0],[x1,y0],[x1,y1],[x0,y1]]
 def deck(id,label,poly,height,stage,color=blue,body=True):
  h=height if callable(height) else lambda x,y:height
  o=obj(id,label,stage,'road');prism(o,poly,h,lambda x,y:h(x,y)-.28,color)
  o['footprint']=poly
  if body:
   p=obj(id+'_BODY',label+' · 实体台柱',stage,'support')
   prism(p,poly,lambda x,y:h(x,y)-.28,lambda x,y:-2,dark if color==blue else color,False)
  return o
 def segment(id,label,a,b,width,stage,body=False,color=blue,trace=True):
  dx,dy=b[0]-a[0],b[1]-a[1];l=math.hypot(dx,dy);nx,ny=-dy/l*width/2,dx/l*width/2
  poly=[[a[0]+nx,a[1]+ny],[a[0]-nx,a[1]-ny],[b[0]-nx,b[1]-ny],[b[0]+nx,b[1]+ny]]
  def h(x,y):return a[2]+((x-a[0])*dx+(y-a[1])*dy)/(l*l)*(b[2]-a[2])
  o=deck(id,label,poly,h,stage,color,body)
  if trace:
   route.append([a[0],a[1],a[2]+.2])
   for i in range(1,9):route.append([a[k]+(b[k]-a[k])*i/8+(.2 if k==2 else 0) for k in range(3)])
  return o
 def marker(id,center,kind,stage):
  # Read the actual first-level faces. Translate only; no rescale or recolor.
  source=(Path(__file__).resolve().parent.parent/'src/roll3_stage_art.h').read_text(encoding='utf-8')
  faces=[]
  for line in source.splitlines():
   vs=re.findall(r'LOCAL\(([-\d.]+)f,([-\d.]+)f,([-\d.]+)f\)',line)
   if len(vs)!=4:continue
   vs=[[float(v) for v in p] for p in vs]
   base=[4.35,16.5,1.55] if kind=='checkpoint' else [0,0,3.7]
   if not all(abs(p[2]-(base[2]+.015))<1e-5 for p in vs):continue
   color=int(re.search(r'0x([0-9a-f]+)',line)[1],16)
   faces.append({'v':[[p[k]-base[k] for k in range(3)] for p in vs],'color':color})
  assert len(faces)==20
  o=obj(id,'第一关同款存档圆盘' if kind=='checkpoint' else '第一关同款起点圆盘',stage,'marker')
  for f in faces:quad(o,*[[p[k]+center[k] for k in range(3)] for p in f['v']],f['color'])
  o['sourceFaces']=faces;o['center']=center
  if kind=='checkpoint':checkpoints.append(center)
 def arrow(id,p,dir,stage):
  dx,dy=dir;l=math.hypot(dx,dy);dx/=l;dy/=l;nx,ny=-dy,dx
  pts=[]
  for u,v in [(-.34,-.09),(.05,-.09),(.05,-.22),(.38,0),(.05,.22),(.05,.09),(-.34,.09)]:
   pts.append([p[0]+u*dx+v*nx,p[1]+u*dy+v*ny])
  o=obj(id,'行进箭头',stage,'marker');prism(o,pts,lambda x,y:p[2]+.018,lambda x,y:p[2]+.012,0xf6d665,False)
 def curve(id,label,c,r,w,radial,sweep,z0,z1,bank,stage):
  def point(t,rr):
   return [c[k]+rr*(math.cos(t)*radial[k]+math.sin(t)*sweep[k]) for k in (0,1)]+[z0+(z1-z0)*(1-math.cos(t))/2+bank*math.sin(t)**2*((rr-r)/w)**2]
  grid=[[point(math.pi*i/32,r+w*(2*j/8-1)) for j in range(9)] for i in range(33)]
  road=obj(id,label,stage,'road');support=obj(id+'_BODY',label+' · 贴合曲面实体塔柱',stage,'support')
  for i in range(32):
   for j in range(8):
    v=[grid[i][j],grid[i+1][j],grid[i+1][j+1],grid[i][j+1]]
    if (v[1][0]-v[0][0])*(v[2][1]-v[0][1])-(v[1][1]-v[0][1])*(v[2][0]-v[0][0])<0:v.reverse()
    quad(road,*v,blue if (i//4+j//2)%2 else 0xffece5)
    low=[[p[0],p[1],p[2]-.28] for p in v];quad(road,*low[::-1],dark);quad(support,*low,dark)
    quad(support,*[[p[0],p[1],-2] for p in v[::-1]],dark)
  boundary=grid[0]+[grid[i][8] for i in range(1,33)]+grid[32][-2::-1]+[grid[i][0] for i in range(31,0,-1)]
  for a,b in zip(boundary,boundary[1:]+boundary[:1]):
   quad(road,a,b,[b[0],b[1],b[2]-.28],[a[0],a[1],a[2]-.28],blue)
   quad(support,[a[0],a[1],a[2]-.28],[b[0],b[1],b[2]-.28],[b[0],b[1],-2],[a[0],a[1],-2],dark)
  for i in range(1,33):
   p=point(math.pi*i/32,r);p[2]+=.2;route.append(p)
  bends.append({'id':id,'center':c,'radius':r,'halfWidth':w,'entry':point(0,r),'exit':point(math.pi,r),'radial':radial,'sweep':sweep})
  # Tiny inner core closes the old large annular void, extended as a tapered spine.
  inner=r-w
  poly=[[c[k]+inner*(math.cos(math.pi*i/16)*radial[k]+math.sin(math.pi*i/16)*sweep[k]) for k in (0,1)] for i in range(17)]
  poly.append([c[k]-1.2*sweep[k] for k in (0,1)])
  o=obj(id+'_SPINE','贴弯尖脊 / 可用作侧墙',stage,'support')
  prism(o,poly,lambda x,y:(z0+z1)/2+.24,lambda x,y:-2,blue,False)

 # High/back to low/front. Bend one opens west; bend two opens north.
 deck('START','起点塔台',rect(-.6,1,4.58,6.22),5.8,0)
 route.append([.2,5.4,6]);marker('START_MARK',[.2,5.4,5.8],'start',0)
 segment('ENTRY','短入口缓坡',[1,5.4,5.8],[3,5.4,5.4],1.64,0)
 curve('CURVE0','紧凑贴柱宽弯',[3,4.25],1.15,.82,[0,1],[1,0],5.4,4.95,.18,0)
 segment('EXIT0','短出弯坡',[3,3.1,4.95],[1.7,3.1,4.65],1.64,0)
 deck('CP1','第一存档塔台',rect(.1,1.7,2.28,3.92),4.65,0)
 route.append([.9,3.1,4.85]);marker('CHECKPOINT1',[.9,3.1,4.65],'checkpoint',0)
 # Two short straight ramps, joined by a level semicircle. The height drop
 # belongs entirely to the ramps, never to the semicircular connector.
 segment('ENTRY1','回转前短斜坡',[.9,2.28,4.65],[.9,1.5,4.2],1.6,1,body=True)
 curve('CURVE1','等高半圆连接',[-.15,1.5],1.05,.8,[1,0],[0,-1],4.2,4.2,0,1)
 segment('EXIT1','回转后短斜坡',[-1.2,1.5,4.2],[-1.2,2.2,3.75],1.6,1,body=True)
 deck('CP2','第二存档塔台',rect(-2,-.4,2.2,3.6),3.75,1)
 route.append([-1.2,2.9,3.95]);marker('CHECKPOINT2',[-1.2,2.9,3.75],'checkpoint',1)
 segment('BRIDGE','短窄桥',[-2,2.9,3.75],[-3.2,2.9,3.75],1.1,1)
 deck('DROP_TOWER','加宽陡降入口塔台',rect(-5.8,-3.2,1.2,3.6),3.75,2)
 route.extend([[-4,2.9,3.95],[-4,1.2,3.95]])
 # First square opening, steep chute towards the front (-Y).
 for i,(a,b) in enumerate([(-4.8,-4.5),(-3.5,-3.2)]):deck('HOLE1_WING'+str(i),'第一方孔侧壁',rect(a,b,.2,1.2),3.75,2)
 # The camera-facing mouth is open; no transverse beam in the ball's sightline.
 segment('CHUTE1','向前陡降槽',[-4,1.2,3.75],[-4,-.5,2.05],1,2)
 deck('RELAY','加宽中继实体塔台',rect(-5.8,-3,-2.3,-.5),2.05,2)
 route.append([-4,-1.2,2.25])
 # Second square opening, orthogonal chute (+X), exits into front goal corner.
 for i,(a,b) in enumerate([(-2.2,-1.7),(-.7,-.2)]):
  top=(lambda x,y:2.05-(x+3)*1.45/1.7) if i==0 else 2.05
  deck('HOLE2_WING'+str(i),'第二方孔前侧斜低边' if i==0 else '第二方孔后侧高墙',rect(-3,-2,a,b),top,3)
 deck('HOLE2_CORNER','第二方孔后角框',rect(-2,-1.88,-.7,-.2),2.05,3)
 segment('CHUTE2','横向陡降槽',[-3,-1.2,2.05],[-1.3,-1.2,.6],1,3)
 goal=[[-1.3,-.5],[1.1,-.5],[1.5,-.9],[1.5,-3.5],[1.1,-3.9],[-.9,-3.9],[-1.3,-3.5]]
 deck('GOAL','绿色终点',goal,.6,3,green)
 route.extend([[-.7,-1.2,.8],[.1,-2.3,.8]])
 # Low goal guards remain green, far from chute mouth.
 for id,poly in [('GOAL_GUARD_E',rect(1.42,1.58,-3.3,-1)),('GOAL_GUARD_S',rect(-.8,1,-4,-3.84))]:
  o=obj(id,'绿色终点护边',3,'guard');prism(o,poly,lambda x,y:.82,lambda x,y:.32,green,False)
 for id,p,d,s in [('A0',[2,5.4,5.6],[1,0],0),('A1',[2.3,3.1,4.79],[-1,0],0),('A2',[-2.6,2.9,3.75],[-1,0],1),('A3',[-4,1.7,3.75],[0,-1],2),('A4',[-3.65,-1.2,2.05],[1,0],2)]:arrow(id,p,d,s)
 return {'route':route,'bends':bends,'checkpoints':checkpoints,'camera':{'yaw':-2.4,'pitch':.8,'distance':9},'stages':['高层紧弯 / 第一存档','转轴紧弯 / 第二存档','前向陡降 / 中继','横向陡降 / 绿色终点']}
