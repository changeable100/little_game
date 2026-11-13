"""Generate the board course from the approved desktop authoring source.

Only tessellation density changes; no independent course reconstruction.
Render/collision faces share vertices and the same authored surfaces.
"""
from pathlib import Path
import json,math,runpy,sys,hashlib
import numpy as np
R=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(R/'tools'))
runpy.run_path(str(R/'tools/build_m4_desktop_model.py'),init_globals={'BOARD_BUILD':True})
from build_m4_desktop_model import triangulate
board=json.loads((R/'build/level2-board-model/model.json').read_text(encoding='utf-8'))
desktop=json.loads((R/'models/level2-desktop/model.json').read_text(encoding='utf-8'))
angle=-2*math.pi/3-desktop['camera']['yaw'];co,si=math.cos(angle),math.sin(angle)
def transform(p):return [round(co*p[0]-si*p[1],6),round(si*p[0]+co*p[1],6),round(p[2],6)]
def vector(p):return transform(p)
vertices=[];lookup={};faces=[];groups=[];collision=[]
DRAW,SOLID,FLOOR,GOAL,CP1,CP2=1,2,4,8,16,32
def vertex(p):
 p=tuple(transform(p))
 if p not in lookup:lookup[p]=len(vertices);vertices.append(p)
 return lookup[p]
def face(points,flags,color):
 ids=[vertex(p) for p in points];v=np.array([vertices[i] for i in ids]);n=np.cross(v[1]-v[0],v[2]-v[0]);length=np.linalg.norm(n)
 if length<1e-9:return
 n=(n/length).tolist();faces.append({'v':ids,'flags':flags,'color':color,'n':n})
 if flags&SOLID:collision.append(len(faces)-1)
def paint(c):return sum(round(c[k]*255)<<(16-k*8) for k in range(3))
def extra(id):return GOAL if id=='GOAL' else CP1 if id=='CP1' else CP2 if id=='CP2' else 64 if id=='CHUTE1' else 128 if id=='CHUTE2' else 0
for o in board['objects']:
 first=len(faces);flat=o['kind']=='road' and 'footprint' in o
 points=[]
 for i in range(0,len(o['p']),9):
  n=o['n'][i:i+3];v=[o['p'][i+k:i+k+3] for k in (0,3,6)];points.extend(v)
  if o['kind']=='marker':
   if n[2]>.1:face(v,DRAW,paint(o['c'][i:i+3]))
   continue
  if o['id'].endswith('_BODY'):
   if abs(n[2])<.1:face(v,DRAW|SOLID,paint(o['c'][i:i+3]))
   continue
  if n[2]<-.1:continue # never draw/collide with buried internal slab bottoms
  flags=DRAW
  if not(flat and n[2]>.1):flags|=SOLID
  if n[2]>.1:flags|=FLOOR|extra(o['id'])
  face(v,flags,paint(o['c'][i:i+3]))
 if flat:
  # Merge checker subdivisions for collision only. These are exactly coplanar.
  i=next(i for i in range(0,len(o['p']),9) if o['n'][i+2]>.1)
  a=np.array(o['p'][i:i+3]);n=np.array(o['n'][i:i+3]);d=float(a@n)
  for t in triangulate(o['footprint']):
   u=np.array(t[1])-t[0];v=np.array(t[2])-t[0]
   if u[0]*v[1]-u[1]*v[0]<0:t=t[::-1]
   face([[x,y,(d-n[0]*x-n[1]*y)/n[2]] for x,y in t],SOLID|FLOOR|extra(o['id']),0)
 count=len(faces)-first
 if count:
  pts=np.array([vertices[k] for f in faces[first:] for k in f['v']]);groups.append({'id':o['id'],'first':first,'count':count,'min':pts.min(axis=0).tolist(),'max':pts.max(axis=0).tolist()})
nodes=[];ordered=[]
def bvh(ids):
 i=len(nodes);points=np.array([vertices[k] for id in ids for k in faces[id]['v']]);lo=points.min(axis=0);hi=points.max(axis=0)
 node={'min':lo.tolist(),'max':hi.tolist(),'first':0,'count':0,'next':0};nodes.append(node)
 if len(ids)<=8:node['first']=len(ordered);node['count']=len(ids);ordered.extend(ids)
 else:
  axis=int(np.argmax(hi-lo));ids.sort(key=lambda id:sum(vertices[v][axis] for v in faces[id]['v']))
  mid=len(ids)//2;bvh(ids[:mid]);bvh(ids[mid:])
 node['next']=len(nodes)
bvh(collision[:])
def fmt(x):
 if abs(x)<.0000005:x=0
 s=f'{x:.7f}'.rstrip('0').rstrip('.')
 return s+('.0' if '.' not in s else '')+'f'
def vec(p):return '{'+','.join(fmt(x) for x in p)+'}'
def zone(center,half=(.5,.5,.35)):
 # Axis-aligned zone encloses the rotated local pad; contact flags disambiguate.
 pts=[transform([center[0]+sx*half[0],center[1]+sy*half[1],center[2]+sz*half[2]]) for sx in (-1,1) for sy in (-1,1) for sz in (-1,1)]
 return '{'+vec([min(p[k] for p in pts) for k in range(3)])+','+vec([max(p[k] for p in pts) for k in range(3)])+'}'
lines=['/* Generated from tools/m4_desktop_layout.py / PC model r4. Do not hand-edit. */','#include "roll3_mesh.h"','#include "roll3_scene.h"']
lines+=['static const r3_vec_t level2_vertices[]={']+[vec(v)+',' for v in vertices]+['};']
lines+=['static const r3_mesh_face_t level2_faces[]={']+['{{'+','.join(map(str,f['v']))+'},'+str(f['flags'])+',0x%06x,'%f['color']+vec(f['n'])+'},' for f in faces]+['};']
lines+=['static const r3_mesh_node_t level2_nodes[]={']+['{'+vec(n['min'])+','+vec(n['max'])+f",{n['first']},{n['count']},{n['next']},0"+'},' for n in nodes]+['};']
lines+=['static const uint16_t level2_collision_ids[]={'+','.join(map(str,ordered))+'};']
lines+=['static const r3_mesh_group_t level2_groups[]={']+['{'+vec(g['min'])+','+vec(g['max'])+f",{g['first']},{g['count']}"+'},' for g in groups]+['};']
lines+=['static const r3_mesh_t level2_mesh={level2_vertices,level2_faces,level2_nodes,level2_collision_ids,level2_groups,'+','.join(map(str,[len(vertices),len(faces),len(nodes),len(collision),len(groups)]))+'};']
cps=[[x,y,z+.2] for x,y,z in board['checkpoints']]
def checkpoint_zone(p):
 center=transform(p)
 return '{'+vec([center[0]-.42,center[1]-.42,center[2]-.3])+','+vec([center[0]+.42,center[1]+.42,center[2]+.3])+'}'
lines+=['static const r3_checkpoint_t level2_checkpoints[]={']+['{'+checkpoint_zone(p)+','+vec(transform(p))+'},' for p in cps]+['};']
lines+=['static const r3_course_style_t level2_style={.background=0x113448,.light_tile=0xffece5,.skirt_z=-2};']
lines+=['static const r3_world_t level2_scene={',f'.spawn={vec(transform(board["route"][0]))},.checkpoint_spawn={vec(transform(cps[0]))},',f'.checkpoint={zone(cps[0])},.goal={zone([.9,-2.2,.8],[1.4,1.7,.3])},', '.fall_z=-3,.style=&level2_style,.finish_rolls=true,.mesh=&level2_mesh,', '.checkpoints=level2_checkpoints,.checkpoint_count=2,.camera_pitch=.8f,.camera_distance=6.0f};', 'const r3_world_t *r3_level2_world(void){return &level2_scene;}']
lines+=['static const r3_vec_t level2_route[]={']+[vec(transform(p))+',' for p in board['route']]+['};', 'const r3_vec_t *r3_level2_route(unsigned *count){if(count)*count=sizeof(level2_route)/sizeof(*level2_route);return level2_route;}']
(R/'src/roll3_scene_level2.c').write_text('\n'.join(lines)+'\n',encoding='utf-8')
# Independent nearest-distance check from every desktop top vertex to the board
# top triangles of that same authored part, before any world rotation.
def distances(points,tris):
 a=tris[:,0];ab=tris[:,1]-a;ac=tris[:,2]-a;n=np.cross(ab,ac);nn=np.sum(n*n,axis=1)
 maximum=0
 for p in points:
  ap=p-a;d=np.sum(ap*n,axis=1)/nn;project=p-d[:,None]*n
  sides=[np.sum(np.cross(tris[:,(j+1)%3]-tris[:,j],project-tris[:,j])*n,axis=1)>=-1e-8 for j in range(3)]
  inside=sides[0]&sides[1]&sides[2];best=np.where(inside,d*d*nn,np.inf)
  for j in range(3):
   e=tris[:,(j+1)%3]-tris[:,j];q=tris[:,j];t=np.clip(np.sum((p-q)*e,axis=1)/np.sum(e*e,axis=1),0,1)
   delta=p-q-t[:,None]*e;best=np.minimum(best,np.sum(delta*delta,axis=1))
  maximum=max(maximum,float(np.sqrt(best.min())))
 return maximum
errors={};parts={o['id']:o for o in board['objects']}
for o in desktop['objects']:
 if o['kind']!='road':continue
 points=np.array([o['p'][i:i+3] for i in range(0,len(o['p']),3) if o['n'][i+2]>.1])
 b=parts[o['id']];tris=np.array([[b['p'][i+j:i+j+3] for j in (0,3,6)] for i in range(0,len(b['p']),9) if b['n'][i+2]>.1])
 errors[o['id']]=distances(points,tris)
assert max(errors.values())<=.01,errors
report={'sourceRevision':desktop['revision'],'sourceSHA256':hashlib.sha256((R/'models/level2-desktop/model.json').read_bytes()).hexdigest(),'vertices':len(vertices),'faces':len(faces),'collisionFaces':len(collision),'bvhNodes':len(nodes),'groups':len(groups),'maxTopErrorM':max(errors.values()),'partErrorsM':errors,'rotationRadians':angle,'route':[transform(p) for p in board['route']]}
(R/'build/level2-port-report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('Board port:',len(vertices),'vertices',len(faces),'faces',len(collision),'collision /',len(nodes),'BVH nodes; max error',max(errors.values()))
