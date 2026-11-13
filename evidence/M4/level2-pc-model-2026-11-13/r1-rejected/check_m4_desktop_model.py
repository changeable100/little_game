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
bridge=parts['DIAGONAL_RETURN'];cp=parts['SAVE'];west=parts['WEST_TURN']
assert abs(bridge['bounds'][0][0]+1)<1e-6 and abs(bridge['bounds'][1][0]-3)<1e-6
assert abs(cp['bounds'][0][0]-3)<1e-6 and abs(west['bounds'][1][0]+1)<1e-6
assert all(abs(o['top']-4.65)<1e-6 for o in [cp,bridge,west])
assert all(p[1]<=-1+1e-6 for p in bridge['footprint'] if p[0]<=1+1e-6)
# Independent read-back of flat top triangles at the three-way connection.
def cross(a,b,c):return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])
def area(p):return abs(sum(a[0]*b[1]-b[0]*a[1] for a,b in zip(p,p[1:]+p[:1]))/2)
def intersect(a,b):
 if cross(*b)<0:b=b[::-1]
 for s,e in zip(b,b[1:]+b[:1]):
  q=[]
  if not a:return []
  for p,n in zip(a[-1:]+a[:-1],a):
   d1=cross(s,e,p);d2=cross(s,e,n)
   if (d1>=-1e-8)!=(d2>=-1e-8):
    t=d1/(d1-d2);q.append([p[k]+t*(n[k]-p[k]) for k in (0,1)])
   if d2>=-1e-8:q.append(n)
  a=q
 return a
def tops(o):
 return [[o['p'][i+j:i+j+2] for j in [0,3,6]] for i in range(0,len(o['p']),9) if all(abs(o['p'][i+j+2]-4.65)<1e-6 for j in [0,3,6]) and o['n'][i+2]>.99]
results=[]
for a,b in [(cp,bridge),(west,bridge),(cp,west)]:
 overlap=sum(area(p) for x in tops(a) for y in tops(b) if len(p:=intersect(x,y))>=3)
 assert overlap<1e-7,(a['id'],b['id'],overlap)
 results.append(f"PASS {a['id']} / {b['id']}: top overlap {overlap:.9f} m2")
blob=(R/'level2-desktop.glb').read_bytes();magic,version,length=struct.unpack_from('<III',blob)
assert magic==0x46546c67 and version==2 and length==len(blob)
jlen,jtype=struct.unpack_from('<II',blob,12);assert jtype==0x4e4f534a
g=json.loads(blob[20:20+jlen]);blen,btype=struct.unpack_from('<II',blob,20+jlen)
assert btype==0x004e4942 and len(blob)==28+jlen+blen
assert len(g['nodes'])==len(parts) and len(g['meshes'])==len(parts)
for view in g['bufferViews']:assert view['byteOffset']+view['byteLength']<=blen
results += [f'PASS {len(parts)} parts / {triangles} non-degenerate exported triangles',
 f'PASS GLB 2.0 buffer ranges and {len(g["nodes"])} nodes',
 'PASS exact-height checkpoint/return/west attachment; start tower horizontal gap >=0.4m',
 'NOT TESTED physics traversal of this PC mesh; board port deferred by user']
text='\n'.join(results)+'\n';(R/'validation.txt').write_text(text,encoding='utf-8');print(text)
