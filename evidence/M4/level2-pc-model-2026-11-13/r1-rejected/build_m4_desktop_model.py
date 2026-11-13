"""Build the PC-first review model, not firmware. Standard library only.

Seed: build/m4-model-seed.json, exported from the last authoring snapshot.
Output: models/level2-desktop, editable geometry JSON, GLB, OBJ and offline viewer.
"""
from pathlib import Path
import json,math,re,struct,hashlib,shutil
R=Path(__file__).resolve().parent.parent
OUT=R/'models/level2-desktop';OUT.mkdir(parents=True,exist_ok=True)
seed_path=OUT/'source-seed.json'
seed=json.loads((seed_path if seed_path.exists() else R/'build/m4-model-seed.json').read_text(encoding='utf-8-sig'))
names=[b['name'] for b in seed['boxes']] if all('name' in b for b in seed['boxes']) else re.findall(r'/\* \d+: (\w+) \*/',(R/'src/roll3_scene_m4.c').read_text(encoding='utf-8'))
EPS=1e-7
objects=[];checks=[]
def area(p):return sum(a[0]*b[1]-b[0]*a[1] for a,b in zip(p,p[1:]+p[:1]))/2
def cross(a,b,c):return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])
def clean(p):
 q=[]
 for a in p:
  if not q or math.dist(a,q[-1])>EPS:q.append(a)
 if len(q)>1 and math.dist(q[0],q[-1])<EPS:q.pop()
 return q
def clip(p,axis,bound,sign):
 q=[]
 for a,b in zip(p[-1:]+p[:-1],p):
  da=(a[axis]-bound)*sign;db=(b[axis]-bound)*sign
  if (da>=-EPS)!=(db>=-EPS):
   t=da/(da-db);q.append([a[0]+t*(b[0]-a[0]),a[1]+t*(b[1]-a[1])])
  if db>=-EPS:q.append(b)
 return clean(q)
def triangulate(p):
 p=clean(p)
 if area(p)<0:p=list(reversed(p))
 idx=list(range(len(p)));result=[]
 while len(idx)>3:
  found=False
  for n in range(len(idx)):
   i,j,k=idx[n-1],idx[n],idx[(n+1)%len(idx)]
   if cross(p[i],p[j],p[k])<EPS:continue
   if any(cross(p[i],p[j],p[t])>EPS and cross(p[j],p[k],p[t])>EPS and cross(p[k],p[i],p[t])>EPS for t in idx if t not in (i,j,k)):continue
   result.append([p[i],p[j],p[k]]);idx.pop(n);found=True;break
  if not found:
   # Remove collinear boundary points, preserving the polygon itself.
   for n in range(len(idx)):
    if abs(cross(p[idx[n-1]],p[idx[n]],p[idx[(n+1)%len(idx)]]))<EPS:idx.pop(n);found=True;break
   if not found:raise ValueError('Non-simple polygon')
 if len(idx)==3:result.append([p[t] for t in idx])
 assert abs(sum(abs(area(t)) for t in result)-abs(area(p)))<1e-5
 return result
def obj(name,label,stage,kind):
 o={'id':name,'name':label,'stage':stage,'kind':kind,'p':[],'n':[],'c':[]};objects.append(o);return o
def rgb(c):return [((c>>s)&255)/255 for s in (16,8,0)]
def tri(o,a,b,c,color):
 u=[b[i]-a[i] for i in range(3)];v=[c[i]-a[i] for i in range(3)]
 n=[u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]]
 length=math.sqrt(sum(x*x for x in n))
 if length<1e-10:return
 n=[x/length for x in n]
 for p in (a,b,c):o['p']+=p;o['n']+=n;o['c']+=rgb(color)
def quad(o,a,b,c,d,color):tri(o,a,b,c,color);tri(o,a,c,d,color)
def tiled(o,poly,height,color):
 for t in triangulate(poly):
  for x in range(math.floor(min(p[0] for p in t)*2),math.ceil(max(p[0] for p in t)*2)):
   for y in range(math.floor(min(p[1] for p in t)*2),math.ceil(max(p[1] for p in t)*2)):
    q=t
    for axis,b,s in [(0,x/2,1),(0,(x+1)/2,-1),(1,y/2,1),(1,(y+1)/2,-1)]:q=clip(q,axis,b,s) if q else []
    if len(q)<3 or abs(area(q))<EPS:continue
    paint=color if (x+y)%2 else 0xffece5
    points=[[u,v,height(u,v)] for u,v in q]
    for i in range(1,len(points)-1):tri(o,points[0],points[i],points[i+1],paint)
def prism(o,poly,top,bottom,color,checker=True):
 poly=clean(poly)
 if area(poly)<0:poly.reverse()
 if checker:tiled(o,poly,top,color)
 else:
  for a,b,c in triangulate(poly):tri(o,[*a,top(*a)],[*b,top(*b)],[*c,top(*c)],color)
 for a,b,c in triangulate(poly):tri(o,[*c,bottom(*c)],[*b,bottom(*b)],[*a,bottom(*a)],color)
 for a,b in zip(poly,poly[1:]+poly[:1]):quad(o,[*a,top(*a)],[*a,bottom(*a)],[*b,bottom(*b)],[*b,top(*b)],color)
def boxpoly(b):
 c=b['center'];h=b['half'];a=b['axes']
 return [[c[k]+i*h[0]*a[0][k]+j*h[1]*a[1][k] for k in (0,1)] for i,j in [(-1,-1),(1,-1),(1,1),(-1,1)]]
labels=['起点塔台','入口缓坡','出弯下坡','金色存档台','绕柱折返连接','西侧转向塔台','横向短坡','第一孔口背台','第一孔口北墙','第一孔口南墙','横向陡降槽','下层中继台','第二孔口左墙','第二孔口右墙','纵向陡降槽']
# Continuous dogleg: exact cuts at u=3 and u=-1. Clears start tower by 0.4m.
d=[-.8,-1.7];l=math.hypot(*d);n=[-d[1]/l,d[0]/l];m=[n[0],n[1]-1];ml=math.hypot(*m);m=[v/ml for v in m];scale=.7/(m[0]*n[0]+m[1]*n[1])
bridge=[[3+.7*n[0],.7*n[1]],[2.2+scale*m[0],-1.7+scale*m[1]],[-1,-2.4],[-1,-1.0],[2.2-scale*m[0],-1.7-scale*m[1]],[3-.7*n[0],-.7*n[1]]]
bridge=clip(clip(bridge,0,3,-1),0,-1,1)
checks.append({'name':'存档台/折返连接','status':'PASS','detail':'连接面切齐 u=3，西转台切齐 u=-1；无共面重叠'})
for i,b in enumerate(seed['boxes']):
 if 15<=i<=19 or i in (30,31):continue
 stage=0 if i<=3 or i in (27,28,29) else 1 if i<=6 else 2 if i<=13 or i in (20,21,25) else 3
 kind='support' if i>=29 else 'guard' if i>=20 else 'road'
 guard_labels={20:'中继台北侧护墙',21:'中继台东侧护墙',22:'终点西侧护墙',23:'终点东侧护墙',24:'终点末端护墙',25:'横向孔口远侧边框',26:'纵向孔口远侧边框',27:'存档台东侧护墙',28:'存档台北侧护墙',29:'存档台偏置支柱'}
 label=labels[i] if i<15 else guard_labels.get(i,names[i])
 o=obj(names[i],label,stage,kind)
 p=boxpoly(b)
 if i==4:
  p=bridge;top=lambda u,v:4.65;bottom=lambda u,v:4.35
 else:
  normal=b['axes'][2];center=b['center'];h=b['half'][2]
  top=lambda u,v,n=normal,c=center,h=h:c[2]+(h-n[0]*(u-c[0])-n[1]*(v-c[1]))/n[2]
  if i==29:bottom=lambda u,v:-2;top=lambda u,v:4.35
  elif abs(normal[2]-1)<1e-5:bottom=lambda u,v,c=center,h=h:c[2]-h
  else:bottom=lambda u,v,t=top:t(u,v)-.3
 deep=abs(min(bottom(*q) for q in p)+2)<EPS and i<20
 if deep:
  body=obj(names[i]+'_BODY',label+' · 实体台柱',stage,'support')
  prism(body,p,lambda u,v,t=top:t(u,v)-.3,bottom,b['color'],False)
  bottom=lambda u,v,t=top:t(u,v)-.3
 prism(o,p,top,bottom,b['color'],i<20)
 o['footprint']=p;o['top']=max(top(*p0) for p0 in p);o['bottom']=min(bottom(*p0) for p0 in p)
# One coherent green pedestal, no internal band walls or duplicate top planes.
rows=[(3.1,-.5+i*.8,1.1 if i in (0,4) else 1.7 if i in (1,3) else 1.9) for i in range(5)]
left=[];right=[]
for u,v,h in rows:left.extend([[u-h,v],[u-h,v+.8]]);right.extend([[u+h,v],[u+h,v+.8]])
goal=clean(left+list(reversed(right)))
o=obj('GOAL','绿色终点台面',3,'road');prism(o,goal,lambda u,v:.6,lambda u,v:.3,0x8ac879);o.update(footprint=goal,top=.6,bottom=.3)
body=obj('GOAL_BODY','绿色终点 · 实体塔柱',3,'support');prism(body,goal,lambda u,v:.3,lambda u,v:-2,0x8ac879,False)
def bp(b,a,r):
 c=math.cos(a);s=math.sin(a);height=b['origin'][2]+.5*(b['exitZ']-b['origin'][2])*(1-c)+s*s*b['trough']*((r-b['radius'])/b['width'])**2
 return [b['origin'][k]+r*(c*b['radial'][k]+s*b['sweep'][k]) for k in (0,1)]+[height]
for bi,b in enumerate(seed['bends']):
 o=obj('CURVE'+str(bi),'纵向宽回头弯' if bi==0 else '横向反向小弯',bi,'road')
 grid=[[bp(b,math.pi*i/24,b['radius']+b['width']*(2*j/8-1)) for j in range(9)] for i in range(25)]
 for i in range(24):
  for j in range(8):
   a,c,d,e=grid[i][j],grid[i+1][j],grid[i+1][j+1],grid[i][j+1]
   # Orient floor normals upward independently of bend handedness.
   if cross(a,c,d)<0:a,c,d,e=e,d,c,a
   quad(o,a,c,d,e,b['color'] if (i+j//2)%2 else 0xffece5)
   a,c,d,e=[[p[0],p[1],p[2]-.3] for p in (a,c,d,e)];quad(o,e,d,c,a,b['color'])
 for lane in [0,8]:
  for i in range(24):
   a=grid[i][lane];c=grid[i+1][lane];quad(o,a,c,[c[0],c[1],c[2]-.3],[a[0],a[1],a[2]-.3],b['color'])
 for row in [0,24]:
  for j in range(8):
   a=grid[row][j];c=grid[row][j+1];quad(o,a,c,[c[0],c[1],c[2]-.3],[a[0],a[1],a[2]-.3],b['color'])
 # Fitted support cap follows the exact underside vertices of this bend.
 support=obj('CURVE_PIER'+str(bi),'宽弯承柱' if bi==0 else '小弯承柱',bi,'support')
 i0,i1,j0,j1=10,14,3,5
 boundary=[grid[i][j0] for i in range(i0,i1+1)]+[grid[i1][j] for j in range(j0+1,j1+1)]+[grid[i][j1] for i in range(i1-1,i0-1,-1)]+[grid[i0][j] for j in range(j1-1,j0,-1)]
 for i in range(i0,i1):
  for j in range(j0,j1):
   v=[[p[0],p[1],p[2]-.3] for p in [grid[i][j],grid[i+1][j],grid[i+1][j+1],grid[i][j+1]]]
   if cross(*v[:3])<0:v.reverse()
   quad(support,*v,0x32768c)
 poly=[[p[0],p[1]] for p in boundary]
 for a,c,d in triangulate(poly):tri(support,[*d,-2],[*c,-2],[*a,-2],0x32768c)
 for a,c in zip(boundary,boundary[1:]+boundary[:1]):quad(support,[a[0],a[1],a[2]-.3],[c[0],c[1],c[2]-.3],[c[0],c[1],-2],[a[0],a[1],-2],0x32768c)
 checks.append({'name':support['name'],'status':'PASS','detail':'柱顶与弯道底网格逐点贴合，无穿入路面'})
for i,f in enumerate(seed['art']):
 stage=(0 if i in (0,1,10,11) or 16<=i<56 else 1 if 2<=i<6 else 2 if i<59 else 3)
 o=obj('ART'+str(i),'方向标记' if i<10 else '存档文字与旗杆',stage,'marker')
 v=[p[:] for p in f['v']]
 # Relocate the return arrow to the corrected, non-overlapping dogleg.
 if i in (2,3):
  for p in v:p[1]-=1
 quad(o,*v,f['color'])
# Audit horizontal same-height coplanar area overlap by triangle clipping.
def convex_intersect(subject,clipper):
 p=subject
 if area(clipper)<0:clipper=clipper[::-1]
 for a,b in zip(clipper,clipper[1:]+clipper[:1]):
  q=[]
  if not p:break
  for c,d in zip(p[-1:]+p[:-1],p):
   dc=cross(a,b,c);dd=cross(a,b,d)
   if (dc>=-EPS)!=(dd>=-EPS):
    t=dc/(dc-dd);q.append([c[k]+t*(d[k]-c[k]) for k in (0,1)])
   if dd>=-EPS:q.append(d)
  p=clean(q)
 return p
def overlap(a,b):
 return sum(abs(area(q)) for ta in triangulate(a) for tb in triangulate(b) if len(q:=convex_intersect(ta,tb))>=3)
oldbridge=boxpoly(seed['boxes'][4]);cp=boxpoly(seed['boxes'][3]);start=boxpoly(seed['boxes'][0])
old_overlap=overlap(oldbridge,cp);new_overlap=overlap(bridge,cp)
assert old_overlap>0 and new_overlap<1e-6
assert overlap(bridge,start)<1e-6
def segment_distance(p,a,b):
 d=[b[k]-a[k] for k in (0,1)];l=sum(v*v for v in d)
 t=max(0,min(1,sum((p[k]-a[k])*d[k] for k in (0,1))/l)) if l else 0
 return math.dist(p,[a[k]+t*d[k] for k in (0,1)])
clearance=min(segment_distance(p,a,b) for poly,other in [(bridge,start),(start,bridge)] for p in poly for a,b in zip(other,other[1:]+other[:1]))
assert clearance>=.4-1e-6
checks.append({'name':'起点柱体净空','status':'PASS','detail':'折返桥靠柱一侧边界v=-1.0，柱边v=-0.6，水平间距0.4m'})
checks.append({'name':'旧交接问题','status':'FIXED','detail':f'旧斜桥侵入存档台 {old_overlap:.3f}m²；电脑模型修正后 {new_overlap:.6f}m²'})
for o in objects:
 pts=[o['p'][i:i+3] for i in range(0,len(o['p']),3)]
 o['bounds']=[[min(p[k] for p in pts) for k in range(3)],[max(p[k] for p in pts) for k in range(3)]]
 o['p']=[round(x,6) for x in o['p']];o['n']=[round(x,5) for x in o['n']];o['c']=[round(x,4) for x in o['c']]
 # Cell clipping can create sub-micron slivers from float seed coordinates.
 # Drop triangles collapsed by the explicit 1um export quantization.
 keep=[]
 for i in range(0,len(o['p']),9):
  a,b,c=[o['p'][i+j:i+j+3] for j in [0,3,6]]
  u=[b[k]-a[k] for k in range(3)];v=[c[k]-a[k] for k in range(3)]
  n=[u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]]
  if sum(x*x for x in n)>1e-16:keep.extend(range(i,i+9))
 for key in ['p','n','c']:o[key]=[o[key][i] for i in keep]
route=[[0,0,6],[0,2.9,5.57]]
for bi,b in enumerate(seed['bends']):
 if bi:route += [[4,0,4.85],[2.5,-1.4,4.85],[1,-1.7,4.85],[-2,-1.7,4.85],[-2,0,4.85],[-3,0,4.85]]
 route += [[*bp(b,math.pi*i/24,b['radius'])[:2],bp(b,math.pi*i/24,b['radius'])[2]+.2] for i in range(25)]
route += [[-.35,-3.4,3.7],[.8,-3.4,3.066],[2.2,-3.4,2.3],[3.1,-3.4,2.3],[3.1,-1.3,1.624],[3.1,1.5,.8]]
model={'revision':'desktop-r1','state':'电脑模型待审阅；未移植、未进行此模型的物理通关验收','objects':objects,'route':route,'checks':checks,'stages':['上层起点 / 存档','折返 / 反向小弯','横向陡降 / 中继','纵向陡降 / 终点']}
(OUT/'model.json').write_text(json.dumps(model,ensure_ascii=False,separators=(',',':')),encoding='utf-8')
(OUT/'geometry-checks.json').write_text(json.dumps(checks,ensure_ascii=False,indent=2),encoding='utf-8')
# Standard glTF binary, Y-up, vertex colors; readily opened in Blender and CAD viewers.
binary=bytearray();views=[];accessors=[];meshes=[];nodes=[]
def accessor(values,kind):
 offset=len(binary);binary.extend(struct.pack('<'+'f'*len(values),*values))
 views.append({'buffer':0,'byteOffset':offset,'byteLength':len(values)*4,'target':34962})
 a={'bufferView':len(views)-1,'componentType':5126,'count':len(values)//3,'type':'VEC3'}
 if kind=='position':a.update(min=[min(values[k::3]) for k in range(3)],max=[max(values[k::3]) for k in range(3)])
 accessors.append(a);return len(accessors)-1
for o in objects:
 p=[];n=[]
 for i in range(0,len(o['p']),3):x,y,z=o['p'][i:i+3];p += [x,z,-y];x,y,z=o['n'][i:i+3];n += [x,z,-y]
 attrs={'POSITION':accessor(p,'position'),'NORMAL':accessor(n,'normal'),'COLOR_0':accessor(o['c'],'color')}
 meshes.append({'name':o['name'],'primitives':[{'attributes':attrs,'material':0}]});nodes.append({'name':o['name'],'mesh':len(meshes)-1,'extras':{'id':o['id'],'stage':o['stage'],'kind':o['kind']}})
gltf={'asset':{'version':'2.0','generator':'M4 PC-first model builder'},'scene':0,'scenes':[{'nodes':list(range(len(nodes)))}],'nodes':nodes,'meshes':meshes,'materials':[{'pbrMetallicRoughness':{'baseColorFactor':[1,1,1,1],'metallicFactor':0,'roughnessFactor':.85},'doubleSided':True}],'buffers':[{'byteLength':len(binary)}],'bufferViews':views,'accessors':accessors}
js=json.dumps(gltf,ensure_ascii=False,separators=(',',':')).encode();js+=b' '*((-len(js))%4);binary+=b'\0'*((-len(binary))%4)
glb=struct.pack('<III',0x46546c67,2,12+8+len(js)+8+len(binary))+struct.pack('<II',len(js),0x4e4f534a)+js+struct.pack('<II',len(binary),0x004e4942)+binary
(OUT/'level2-desktop.glb').write_bytes(glb)
lines=['# M4 desktop model r1; Z-up metres; RGB vertices'];offset=1
for o in objects:
 lines.append('o '+o['id'])
 for i in range(0,len(o['p']),3):lines.append('v '+' '.join(map(str,o['p'][i:i+3]+o['c'][i:i+3])))
 for i in range(len(o['p'])//9):lines.append(f'f {offset+3*i} {offset+3*i+1} {offset+3*i+2}')
 offset+=len(o['p'])//3
(OUT/'level2-desktop.obj').write_text('\n'.join(lines)+'\n',encoding='utf-8')
template=(R/'tools/m4_desktop_viewer.html').read_text(encoding='utf-8')
(OUT/'index.html').write_text(template.replace('__MODEL_DATA__',json.dumps(model,ensure_ascii=False,separators=(',',':'))),encoding='utf-8')
print('Desktop model:',len(objects),'parts,',sum(len(o['p'])//9 for o in objects),'triangles')
print('Old checkpoint overlap',old_overlap,'new',new_overlap,'GLB',len(glb),'bytes')
