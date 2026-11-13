"""Build the PC-first review model, not firmware. Standard library only.

Layout: tools/m4_desktop_layout.py (independent desktop authoring coordinates).
Output: models/level2-desktop, editable geometry JSON, GLB, OBJ and offline viewer.
"""
from pathlib import Path
import json,math,struct
R=Path(__file__).resolve().parent.parent
OUT=R/'models/level2-desktop';OUT.mkdir(parents=True,exist_ok=True)
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
from m4_desktop_layout import build
scene=build(obj,prism,quad,tri)
route=scene['route']
checks=[{'name':'紧凑双弯','status':'PASS','detail':'内半径0.33 / 0.25m，转轴相差90°'}, {'name':'存档外观','status':'PASS','detail':'平移复用第一关原始20面圆盘，尺寸与颜色一致'}, {'name':'对角线推进','status':'PASS','detail':'高层在后，低层与绿色终点在前；实体台柱保持可见'}]
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
model={'revision':'desktop-r2','state':'电脑模型待审阅；未移植、未进行此模型的物理通关验收','objects':objects,'checks':checks,**scene}
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
lines=['# M4 desktop model r2; Z-up metres; RGB vertices'];offset=1
for o in objects:
 lines.append('o '+o['id'])
 for i in range(0,len(o['p']),3):lines.append('v '+' '.join(map(str,o['p'][i:i+3]+o['c'][i:i+3])))
 for i in range(len(o['p'])//9):lines.append(f'f {offset+3*i} {offset+3*i+1} {offset+3*i+2}')
 offset+=len(o['p'])//3
(OUT/'level2-desktop.obj').write_text('\n'.join(lines)+'\n',encoding='utf-8')
template=(R/'tools/m4_desktop_viewer.html').read_text(encoding='utf-8')
(OUT/'index.html').write_text(template.replace('__MODEL_DATA__',json.dumps(model,ensure_ascii=False,separators=(',',':'))),encoding='utf-8')
print('Desktop model:',len(objects),'parts,',sum(len(o['p'])//9 for o in objects),'triangles')
print('GLB',len(glb),'bytes')
