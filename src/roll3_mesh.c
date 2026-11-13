#include "roll3_mesh.h"
#include <math.h>
#include <float.h>
static r3_vec_t add(r3_vec_t a,r3_vec_t b){return (r3_vec_t){a.x+b.x,a.y+b.y,a.z+b.z};}
static r3_vec_t sub(r3_vec_t a,r3_vec_t b){return (r3_vec_t){a.x-b.x,a.y-b.y,a.z-b.z};}
static r3_vec_t mul(r3_vec_t a,float s){return (r3_vec_t){a.x*s,a.y*s,a.z*s};}
static float dot(r3_vec_t a,r3_vec_t b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static r3_vec_t cross(r3_vec_t a,r3_vec_t b){return (r3_vec_t){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static bool bounded(r3_vec_t p){return isfinite(p.x)&&isfinite(p.y)&&isfinite(p.z)&&fabsf(p.x)<=100&&fabsf(p.y)<=100&&fabsf(p.z)<=100;}
static bool overlap(r3_vec_t lo,r3_vec_t hi,r3_vec_t a,r3_vec_t b){return lo.x<=b.x&&hi.x>=a.x&&lo.y<=b.y&&hi.y>=a.y&&lo.z<=b.z&&hi.z>=a.z;}
static bool in_bounds(r3_vec_t p,r3_vec_t a,r3_vec_t b){return p.x>=a.x-.00001f&&p.x<=b.x+.00001f&&p.y>=a.y-.00001f&&p.y<=b.y+.00001f&&p.z>=a.z-.00001f&&p.z<=b.z+.00001f;}
static r3_vec_t closest(r3_vec_t p,r3_vec_t a,r3_vec_t b,r3_vec_t c){
    r3_vec_t ab=sub(b,a),ac=sub(c,a),ap=sub(p,a);float d1=dot(ab,ap),d2=dot(ac,ap);
    if(d1<=0&&d2<=0)return a;
    r3_vec_t bp=sub(p,b);float d3=dot(ab,bp),d4=dot(ac,bp);
    if(d3>=0&&d4<=d3)return b;
    float vc=d1*d4-d3*d2;if(vc<=0&&d1>=0&&d3<=0)return add(a,mul(ab,d1/(d1-d3)));
    r3_vec_t cp=sub(p,c);float d5=dot(ab,cp),d6=dot(ac,cp);
    if(d6>=0&&d5<=d6)return c;
    float vb=d5*d2-d1*d6;if(vb<=0&&d2>=0&&d6<=0)return add(a,mul(ac,d2/(d2-d6)));
    float va=d3*d6-d5*d4;if(va<=0&&d4-d3>=0&&d5-d6>=0)return add(b,mul(sub(c,b),(d4-d3)/((d4-d3)+(d5-d6))));
    float inverse=1/(va+vb+vc);return add(a,add(mul(ab,vb*inverse),mul(ac,vc*inverse)));
}
bool r3_mesh_validate(const r3_mesh_t *m){
    if(!m||!m->vertices||!m->faces||!m->nodes||!m->collision_ids||!m->groups||!m->vertex_count||m->vertex_count>8192||!m->face_count||m->face_count>8192||!m->node_count||m->node_count>4096||!m->collision_count||m->collision_count>m->face_count||!m->group_count||m->group_count>128)return false;
    for(unsigned i=0;i<m->vertex_count;++i)if(!bounded(m->vertices[i]))return false;
    for(unsigned i=0;i<m->face_count;++i){
        const r3_mesh_face_t *f=&m->faces[i];
        if(f->v[0]>=m->vertex_count||f->v[1]>=m->vertex_count||f->v[2]>=m->vertex_count||f->flags>255||f->color>0xffffff||!bounded(f->normal)||fabsf(dot(f->normal,f->normal)-1)>.0001f)return false;
        r3_vec_t n=cross(sub(m->vertices[f->v[1]],m->vertices[f->v[0]]),sub(m->vertices[f->v[2]],m->vertices[f->v[0]]));
        float length=sqrtf(dot(n,n));if(length<.0000001f||dot(n,f->normal)/length<.9999f)return false;
    }
    for(unsigned i=0;i<m->node_count;++i){
        const r3_mesh_node_t *n=&m->nodes[i];
        if(!bounded(n->min)||!bounded(n->max)||n->min.x>n->max.x||n->min.y>n->max.y||n->min.z>n->max.z||n->next<=i||n->next>m->node_count||(unsigned)n->first+n->count>m->collision_count)return false;
        if(n->count){for(unsigned k=0;k<n->count;++k){
            unsigned id=m->collision_ids[n->first+k];if(id>=m->face_count||!(m->faces[id].flags&R3_MESH_SOLID))return false;
            for(unsigned v=0;v<3;++v)if(!in_bounds(m->vertices[m->faces[id].v[v]],n->min,n->max))return false;
        }}else{
            if(i+1>=n->next)return false;
            unsigned right=m->nodes[i+1].next;if(right>=n->next)return false;
            if(m->nodes[right].next!=n->next)return false;
            for(unsigned c=0;c<2;++c){const r3_mesh_node_t *child=&m->nodes[c?right:i+1];if(!in_bounds(child->min,n->min,n->max)||!in_bounds(child->max,n->min,n->max))return false;}
        }
    }
    for(unsigned i=0;i<m->group_count;++i){const r3_mesh_group_t *g=&m->groups[i];if(!g->count||(unsigned)g->first+g->count>m->face_count||!bounded(g->min)||!bounded(g->max))return false;}
    return m->nodes[0].next==m->node_count;
}
r3_mesh_hit_t r3_mesh_nearest(const r3_mesh_t *m,r3_vec_t p,float reach){
    r3_mesh_hit_t hit={.gap=FLT_MAX};if(!m)return hit;
    r3_vec_t ext={reach,reach,reach},lo=sub(p,ext),hi=add(p,ext);float best=reach*reach;
    for(unsigned i=0;i<m->node_count;){const r3_mesh_node_t *node=&m->nodes[i];
        if(!overlap(lo,hi,node->min,node->max)){i=node->next;continue;}
        for(unsigned j=0;j<node->count;++j){const r3_mesh_face_t *f=&m->faces[m->collision_ids[node->first+j]];
            r3_vec_t q=closest(p,m->vertices[f->v[0]],m->vertices[f->v[1]],m->vertices[f->v[2]]),delta=sub(p,q);float d=dot(delta,delta);
            if(d>best)continue;best=d;float distance=sqrtf(d);hit=(r3_mesh_hit_t){.found=true,.gap=distance-R3_RADIUS,.normal=distance>1e-8f?mul(delta,1/distance):f->normal,.flags=f->flags};
        }++i;
    }return hit;
}
static void candidate(r3_mesh_hit_t *h,r3_vec_t delta,float t,r3_vec_t n,uint16_t flags){
    if(t<-.000001f||t>h->t||t>1||dot(delta,n)>=-1e-8f)return;
    h->found=true;h->t=fmaxf(0,t);h->normal=n;h->flags=flags;
}
static bool inside(r3_vec_t p,r3_vec_t a,r3_vec_t b,r3_vec_t c,r3_vec_t n){return dot(cross(sub(b,a),sub(p,a)),n)>=-.000001f&&dot(cross(sub(c,b),sub(p,b)),n)>=-.000001f&&dot(cross(sub(a,c),sub(p,c)),n)>=-.000001f;}
static void sphere_root(r3_mesh_hit_t *h,r3_vec_t start,r3_vec_t delta,r3_vec_t point,uint16_t flags){
    r3_vec_t u=sub(start,point);float aa=dot(delta,delta),bb=dot(u,delta),cc=dot(u,u)-R3_RADIUS*R3_RADIUS,d=bb*bb-aa*cc;
    if(aa<1e-14f||d<0)return;float t=(-bb-sqrtf(d))/aa;
    r3_vec_t n=mul(sub(add(start,mul(delta,t)),point),1/R3_RADIUS);candidate(h,delta,t,n,flags);
}
static void edge_root(r3_mesh_hit_t *h,r3_vec_t start,r3_vec_t delta,r3_vec_t a,r3_vec_t b,uint16_t flags){
    r3_vec_t e=sub(b,a),p=sub(start,a);float ee=dot(e,e),pe=dot(p,e)/ee,de=dot(delta,e)/ee;
    r3_vec_t pp=sub(p,mul(e,pe)),dd=sub(delta,mul(e,de));
    float aa=dot(dd,dd),bb=dot(pp,dd),cc=dot(pp,pp)-R3_RADIUS*R3_RADIUS,disc=bb*bb-aa*cc;
    if(aa<1e-14f||disc<0)return;float t=(-bb-sqrtf(disc))/aa,u=pe+t*de;
    if(u<0||u>1)return;r3_vec_t n=mul(add(pp,mul(dd,t)),1/R3_RADIUS);candidate(h,delta,t,n,flags);
}
r3_mesh_hit_t r3_mesh_sweep(const r3_mesh_t *m,r3_vec_t p,r3_vec_t d,bool skip_floor){
    r3_mesh_hit_t hit={.t=1};if(!m)return hit;
    float r=R3_RADIUS+.00001f;
    r3_vec_t lo={fminf(p.x,p.x+d.x)-r,fminf(p.y,p.y+d.y)-r,fminf(p.z,p.z+d.z)-r};
    r3_vec_t hi={fmaxf(p.x,p.x+d.x)+r,fmaxf(p.y,p.y+d.y)+r,fmaxf(p.z,p.z+d.z)+r};
    for(unsigned i=0;i<m->node_count;){const r3_mesh_node_t *node=&m->nodes[i];
        if(!overlap(lo,hi,node->min,node->max)){i=node->next;continue;}
        for(unsigned j=0;j<node->count;++j){const r3_mesh_face_t *f=&m->faces[m->collision_ids[node->first+j]];
            if(skip_floor&&(f->flags&R3_MESH_FLOOR))continue;
            r3_vec_t v[3]={m->vertices[f->v[0]],m->vertices[f->v[1]],m->vertices[f->v[2]]};
            float speed=dot(d,f->normal),distance=dot(sub(p,v[0]),f->normal);
            if(fabsf(speed)>1e-10f)for(int sign=-1;sign<=1;sign+=2){
                if(speed*sign>=-1e-8f)continue;float t=(sign*R3_RADIUS-distance)/speed;
                if(t<-.000001f||t>hit.t)continue;r3_vec_t n=mul(f->normal,(float)sign);
                r3_vec_t contact=sub(add(p,mul(d,t)),mul(n,R3_RADIUS));
                if(inside(contact,v[0],v[1],v[2],f->normal))candidate(&hit,d,t,n,f->flags);
            }
            for(unsigned k=0;k<3;++k){edge_root(&hit,p,d,v[k],v[(k+1)%3],f->flags);sphere_root(&hit,p,d,v[k],f->flags);}
        }++i;
    }return hit;
}
bool r3_mesh_support_move(const r3_mesh_t *m,r3_vec_t p,r3_vec_t velocity,float *duration){
    if(!m)return false;r3_mesh_hit_t a=r3_mesh_nearest(m,p,R3_RADIUS+.002f);
    if(!a.found||!(a.flags&R3_MESH_FLOOR)||a.gap<-.00001f||a.gap>.001f||a.normal.z<=.25f||fabsf(dot(velocity,a.normal))>.15f)return false;
    float span=*duration;
    for(unsigned i=0;i<5;++i){r3_mesh_hit_t b=r3_mesh_nearest(m,add(p,mul(velocity,span)),R3_RADIUS+.002f);
        if(!b.found||!(b.flags&R3_MESH_FLOOR))return false;
        if(b.gap>=-.001f&&b.gap<=.001f&&dot(a.normal,b.normal)>.995f){*duration=span;return true;}span*=.5f;
    }return false;
}
