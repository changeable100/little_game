"""PC model checks only: no board build, flash or physical-play claim."""
from pathlib import Path
import json,math,struct
R=Path(__file__).resolve().parent.parent/'models/level2-desktop'
m=json.loads((R/'model.json').read_text(encoding='utf-8'))
parts={o['id']:o for o in m['objects']};assert len(parts)==len(m['objects'])
triangles=0
for o in parts.values():
 assert len(o['p'])==len(o['n'])==len(o['c']) and len(o['p'])%9==0
 assert all(math.isfinite(x) for k in ['p','n','c'] for x in o[k])
 assert all(0<=x<=1 for x in o['c'])
 for i in range(0,len(o['n']),3):assert abs(sum(x*x for x in o['n'][i:i+3])-1)<.0001
 for i in range(0,len(o['p']),9):
  a,b,c=[o['p'][i+j:i+j+3] for j in [0,3,6]]
  u=[b[k]-a[k] for k in range(3)];v=[c[k]-a[k] for k in range(3)]
  normal=[u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]]
  assert sum(x*x for x in normal)>1e-16,(o['id'],i)
 triangles+=len(o['p'])//9
# Read back exported geometry, rather than trusting authoring PASS labels.
import re
bounds=[[min(o['bounds'][0][k] for o in parts.values()) for k in range(3)], [max(o['bounds'][1][k] for o in parts.values()) for k in range(3)]]
assert all(bounds[1][k]-bounds[0][k]<=11 for k in (0,1)),bounds
for b in m['bends']:
 assert b['radius']-b['halfWidth']<=.35 and b['halfWidth']*2>=1.5
assert abs(sum(a*b for a,b in zip(m['bends'][0]['radial'],m['bends'][1]['radial'])))<1e-8
# Positive-area overlap between distinct drivable decks is forbidden, even at
# different heights: the old model stacked the goal under upper platforms.
def cross(a,b,c):return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])
def overlap_area(a,b):
 if cross(*b)<0:b=b[::-1]
 for s,e in zip(b,b[1:]+b[:1]):
  q=[]
  if not a:return 0
  for p,n in zip(a[-1:]+a[:-1],a):
   d1=cross(s,e,p);d2=cross(s,e,n)
   if (d1>=-1e-8)!=(d2>=-1e-8):
    f=d1/(d1-d2);q.append([p[k]+f*(n[k]-p[k]) for k in (0,1)])
   if d2>=-1e-8:q.append(n)
  a=q
 return abs(sum(p[0]*q[1]-q[0]*p[1] for p,q in zip(a,a[1:]+a[:1])))/2
def top_triangles(o):
 return [[o['p'][i+j:i+j+2] for j in (0,3,6)] for i in range(0,len(o['p']),9) if o['n'][i+2]>.1]
def separated(a,b):
 return any(max(p[k] for p in a)<=min(p[k] for p in b)+1e-8 or max(p[k] for p in b)<=min(p[k] for p in a)+1e-8 for k in (0,1))
roads=[(o,top_triangles(o)) for o in parts.values() if o['kind']=='road']
max_overlap=0
for i,(a,ta) in enumerate(roads):
 for b,tb in roads[i+1:]:
  if any(a['bounds'][1][k]<=b['bounds'][0][k]+1e-8 or b['bounds'][1][k]<=a['bounds'][0][k]+1e-8 for k in (0,1)):continue
  overlap=sum(overlap_area(x,y) for x in ta for y in tb if not separated(x,y))
  assert overlap<1e-7,(a['id'],b['id'],overlap)
  max_overlap=max(max_overlap,overlap)
source=(R.parent.parent/'src/roll3_stage_art.h').read_text(encoding='utf-8')
expected=[]
for line in source.splitlines():
 vs=re.findall(r'LOCAL\(([-\d.]+)f,([-\d.]+)f,([-\d.]+)f\)',line)
 if len(vs)!=4:continue
 vs=[[float(v) for v in p] for p in vs]
 if all(abs(p[2]-1.565)<1e-5 for p in vs):
  expected.append({'v':[[p[k]-[4.35,16.5,1.55][k] for k in range(3)] for p in vs], 'color':int(re.search(r'0x([0-9a-f]+)',line)[1],16)})
assert len(expected)==20
for id in ['CHECKPOINT1','CHECKPOINT2']:
 assert parts[id]['sourceFaces']==expected
 assert len(parts[id]['p'])==30*9 # 10 rim quads and 10 inner triangles
 cp=parts[id]
 actual={tuple(cp['p'][i:i+3]+cp['c'][i:i+3]) for i in range(0,len(cp['p']),3)}
 required=set()
 for f in expected:
  color=[round(((f['color']>>s)&255)/255,4) for s in (16,8,0)]
  required.update(tuple([round(p[k]+cp['center'][k],6) for k in range(3)]+color) for p in f['v'])
 assert actual==required,(id,'exported vertices/colors must match first-level art')
# Vectorized independent ray tests against actual exported triangles.
import numpy as np
mesh=[];owner=[]
for o in parts.values():
 if o['kind']=='marker':continue
 vs=np.asarray(o['p']).reshape(-1,3,3)
 mesh.extend(vs);owner.extend([o['id']]*len(vs))
t=np.asarray(mesh);v0=t[:,0];e1=t[:,1]-v0;e2=t[:,2]-v0
cam=m['camera'];yaw,pitch=cam['yaw'],cam['pitch']
offset=np.array([math.cos(yaw)*math.cos(pitch),math.sin(yaw)*math.cos(pitch),math.sin(pitch)])*cam['distance']
right=np.array([-math.sin(yaw),math.cos(yaw),0]);up=np.cross(offset/np.linalg.norm(offset),right)
advance=np.array(m['route'][-1])-m['route'][0]
assert float(advance@right)>2 and float(advance@up)<-2,'Overview must advance upper-left to lower-right'
def hits(origin,end):
 d=end-origin;h=np.cross(np.broadcast_to(d,e2.shape),e2);det=np.sum(e1*h,axis=1)
 valid=np.abs(det)>1e-8;inv=np.divide(1,det,out=np.zeros_like(det),where=valid)
 s=origin-v0;u=np.sum(s*h,axis=1)*inv;q=np.cross(s,e1);v=np.sum(d*q,axis=1)*inv
 ray_t=np.sum(e2*q,axis=1)*inv
 ids=np.where(valid & (u>=-1e-6) & (v>=-1e-6) & (u+v<=1+1e-6) & (ray_t>1e-5) & (ray_t<1-1e-4))[0]
 return ids,ray_t[ids]
samples=[]
for a,b in zip(m['route'],m['route'][1:]):
 n=max(1,math.ceil(math.dist(a,b)/.1))
 for i in range(n):samples.append(np.array(a)+(np.array(b)-a)*i/n)
samples.append(np.array(m['route'][-1]))
blocked=[];floor_errors=[]
for i,p in enumerate(samples):
 for label,delta in [('center',np.zeros(3)),('left',right*.14),('right',-right*.14),('top',up*.14)]:
  ids,ts=hits(p+offset,p+delta)
  if len(ids):blocked.append({'sample':i,'point':p.tolist(),'ray':label,'part':owner[ids[np.argmin(ts)]]})
 # Floor within 0.4 mm of nominal sphere contact (curve is piecewise planar).
 ids,ts=hits(p+np.array([0,0,.01]),p-np.array([0,0,.5]))
 height=p[2]+.01-float(min(ts))*.51 if len(ts) else -999
 # Sampling centerline linear chords can lie up to 2mm below a smooth bend.
 if abs(height-(p[2]-.2))>.004:floor_errors.append({'sample':i,'point':p.tolist(),'floor':height})
report={'camera':cam,'samples':len(samples),'rays':len(samples)*4,'blocked':blocked,'floorErrors':floor_errors}
(R/'visibility-checks.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
results=[f'PASS compact footprint {bounds[1][0]-bounds[0][0]:.3f} x {bounds[1][1]-bounds[0][1]:.3f} m', 'PASS two tight bends with orthogonal axes', 'PASS both checkpoint meshes reuse exact first-level vertices and colors']
results.append(f'PASS all {len(roads)} drivable decks: no positive-area plan overlap ({max_overlap:.9f} m2 maximum)')
results.append('PASS overview start/end advance upper-left to lower-right')
print('Visibility',len(blocked),'blocked /',len(samples)*4,'rays; floor errors',len(floor_errors))
for f in blocked[:10]:print('BLOCKED',f)
for f in floor_errors[:10]:print('FLOOR',f)
assert not blocked,'See visibility-checks.json; preserve failure before fixing'
assert not floor_errors,'See visibility-checks.json; preserve failure before fixing'
results.append(f'PASS {len(samples)} route positions / {len(samples)*4} visible sphere rays; continuous floor')
blob=(R/'level2-desktop.glb').read_bytes();magic,version,length=struct.unpack_from('<III',blob)
assert magic==0x46546c67 and version==2 and length==len(blob)
jlen,jtype=struct.unpack_from('<II',blob,12);assert jtype==0x4e4f534a
g=json.loads(blob[20:20+jlen]);blen,btype=struct.unpack_from('<II',blob,20+jlen)
assert btype==0x004e4942 and len(blob)==28+jlen+blen
assert len(g['nodes'])==len(parts) and len(g['meshes'])==len(parts)
for view in g['bufferViews']:assert view['byteOffset']+view['byteLength']<=blen
results += [f'PASS {len(parts)} parts / {triangles} non-degenerate exported triangles',
 f'PASS GLB 2.0 buffer ranges and {len(g["nodes"])} nodes',

 'NOT TESTED physics traversal of this PC mesh; board port deferred by user']
text='\n'.join(results)+'\n';(R/'validation.txt').write_text(text,encoding='utf-8');print(text)
