#include "roll3_renderer.h"
#include "roll3_physics.h"
#include <math.h>
#include <stddef.h>

#define NEAR_Z .15f
#define FAR_Z 30.0f
#define FOCAL 144.0737f /* 150 / (2*tan(55deg/2)) */
static r3_vec_t add(r3_vec_t a,r3_vec_t b){return (r3_vec_t){a.x+b.x,a.y+b.y,a.z+b.z};}
static r3_vec_t sub(r3_vec_t a,r3_vec_t b){return (r3_vec_t){a.x-b.x,a.y-b.y,a.z-b.z};}
static r3_vec_t mul(r3_vec_t a,float s){return (r3_vec_t){a.x*s,a.y*s,a.z*s};}
static float dot(r3_vec_t a,r3_vec_t b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static r3_vec_t cross(r3_vec_t a,r3_vec_t b){return (r3_vec_t){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static bool finite_vec(r3_vec_t p){return isfinite(p.x)&&isfinite(p.y)&&isfinite(p.z);}
static r3_vec_t camera(r3_vec_t p,r3_vec_t focus)
{
    r3_vec_t v=sub(p,add(focus,(r3_vec_t){0,-3.2f,2.6f}));
    float length=sqrtf(3.2f*3.2f+2.6f*2.6f),fy=3.2f/length,fz=-2.6f/length;
    return (r3_vec_t){v.x,-fz*v.y+fy*v.z,fy*v.y+fz*v.z};
}
bool roll3_renderer_project_focus(r3_vec_t p,r3_vec_t focus,float *x,float *y,float *depth)
{
    if(!x||!y||!depth||!finite_vec(p)||!finite_vec(focus))return false;
    p=camera(p,focus);
    if(!finite_vec(p)||p.z<NEAR_Z||p.z>FAR_Z)return false;
    *x=R3_VIEW_W*.5f+FOCAL*p.x/p.z;
    *y=R3_VIEW_H*.5f-FOCAL*p.y/p.z;*depth=p.z;
    return isfinite(*x)&&isfinite(*y);
}
bool roll3_renderer_project(r3_vec_t p,float *x,float *y,float *depth)
{return roll3_renderer_project_focus(p,(r3_vec_t){0,0,R3_RADIUS},x,y,depth);}
r3_vec_t roll3_renderer_focus(const r3_snapshot_t *s,const r3_world_t *world)
{
    r3_vec_t f=s->state==R3_FALLEN ? (s->checkpoint?world->checkpoint_spawn:world->spawn) : s->position;
    f.z=fmaxf(R3_RADIUS,f.z);
    return f;
}
static float follow_axis(float from,float to,float alpha,float deadband)
{
    float delta=to-from;
    if(fabsf(delta)<=deadband)return from;
    return from+alpha*(delta-copysignf(deadband,delta));
}
static r3_vec_t follow_update(r3_camera_follow_t *c,const r3_snapshot_t *s,const r3_world_t *w)
{
    r3_vec_t target=roll3_renderer_focus(s,w);
    uint32_t dt=s->elapsed_ms-c->elapsed_ms;
    if(!c->valid||c->run!=s->run||c->state!=s->state||dt>250){
        c->focus=target;
    }else if(dt){
        float xy=dt/(120.0f+dt),z=dt/(200.0f+dt);
        c->focus.x=follow_axis(c->focus.x,target.x,xy,.025f);
        c->focus.y=follow_axis(c->focus.y,target.y,xy,.025f);
        c->focus.z=follow_axis(c->focus.z,target.z,z,.04f);
    }
    c->valid=true;c->run=s->run;c->state=s->state;c->elapsed_ms=s->elapsed_ms;
    return c->focus;
}
static uint16_t rgb(uint32_t c,float light)
{
    unsigned r=(unsigned)(((c>>16)&255)*light),g=(unsigned)(((c>>8)&255)*light),b=(unsigned)((c&255)*light);
    return (uint16_t)(((r>>3)<<11)|((g>>2)<<5)|(b>>3));
}
static float edge(float ax,float ay,float bx,float by,float x,float y)
{return (x-ax)*(by-ay)-(y-ay)*(bx-ax);}
static void raster(uint16_t *color,uint16_t *depth,r3_vec_t a,r3_vec_t b,r3_vec_t c,uint16_t paint)
{
    float x[3],y[3],iz[3];r3_vec_t v[3]={a,b,c};
    for(unsigned i=0;i<3;++i){iz[i]=1/v[i].z;x[i]=R3_VIEW_W*.5f+FOCAL*v[i].x*iz[i];y[i]=R3_VIEW_H*.5f-FOCAL*v[i].y*iz[i];if(!isfinite(x[i])||!isfinite(y[i]))return;}
    float area=edge(x[0],y[0],x[1],y[1],x[2],y[2]);
    if(!isfinite(area)||fabsf(area)<1e-5f)return;
    float minx=fminf(x[0],fminf(x[1],x[2])),maxx=fmaxf(x[0],fmaxf(x[1],x[2]));
    float miny=fminf(y[0],fminf(y[1],y[2])),maxy=fmaxf(y[0],fmaxf(y[1],y[2]));
    if(maxx<0||minx>=R3_VIEW_W||maxy<0||miny>=R3_VIEW_H)return;
    int left=(int)fmaxf(0,floorf(minx)),right=(int)fminf(R3_VIEW_W-1,ceilf(maxx));
    int top=(int)fmaxf(0,floorf(miny)),bottom=(int)fminf(R3_VIEW_H-1,ceilf(maxy));
    for(int py=top;py<=bottom;++py)for(int px=left;px<=right;++px){
        float u=edge(x[1],y[1],x[2],y[2],px+.5f,py+.5f)/area;
        float v0=edge(x[2],y[2],x[0],y[0],px+.5f,py+.5f)/area;
        float w=1-u-v0;
        if(u<-.00001f||v0<-.00001f||w<-.00001f)continue;
        float invz=u*iz[0]+v0*iz[1]+w*iz[2];if(invz<=0||!isfinite(invz))continue;
        float z=1/invz;if(z<NEAR_Z-.001f||z>FAR_Z+.001f)continue;
        uint16_t d=(uint16_t)(fmaxf(0,fminf(65534,(z-NEAR_Z)/(FAR_Z-NEAR_Z)*65534)));
        unsigned index=(unsigned)py*R3_VIEW_W+(unsigned)px;
        if(d<depth[index]){depth[index]=d;color[index]=paint;}
    }
}
static unsigned clip(const r3_vec_t *in,unsigned n,r3_vec_t *out,float plane,bool above)
{
    unsigned count=0;
    for(unsigned i=0;i<n;++i){r3_vec_t a=in[i],b=in[(i+1)%n];
        bool ai=above?a.z>=plane:a.z<=plane,bi=above?b.z>=plane:b.z<=plane;
        if(ai)out[count++]=a;
        if(ai!=bi){float t=(plane-a.z)/(b.z-a.z);out[count++]=add(a,mul(sub(b,a),t));}
    }
    return count;
}
static void triangle(uint16_t *color,uint16_t *depth,r3_vec_t a,r3_vec_t b,r3_vec_t c,uint32_t paint,r3_vec_t focus)
{
    r3_vec_t normal=cross(sub(b,a),sub(c,a));float length=sqrtf(dot(normal,normal));
    if(!isfinite(length)||length<1e-8f)return;
    float light=.35f+.65f*fabsf(dot(mul(normal,1/length),(r3_vec_t){.3f,-.4f,.8660254f}));
    r3_vec_t p[8]={camera(a,focus),camera(b,focus),camera(c,focus)},temp[8];
    if(!finite_vec(p[0])||!finite_vec(p[1])||!finite_vec(p[2]))return;
    unsigned n=clip(p,3,temp,NEAR_Z,true);if(n<3)return;
    n=clip(temp,n,p,FAR_Z,false);if(n<3)return;
    uint16_t shade=rgb(paint,light);
    for(unsigned i=1;i+1<n;++i)raster(color,depth,p[0],p[i],p[i+1],shade);
}
static r3_vec_t rotate(r3_quat_t q,r3_vec_t p)
{
    r3_vec_t u={q.x,q.y,q.z};r3_vec_t t=mul(cross(u,p),2);
    return add(p,add(mul(t,q.w),cross(u,t)));
}
static r3_vec_t sphere_point(unsigned lat,unsigned lon,float r,r3_quat_t q,r3_vec_t pos)
{
    float a=3.14159265f*lat/6,b=6.2831853f*(lon%12)/12;
    return add(pos,rotate(q,(r3_vec_t){r*sinf(a)*cosf(b),r*sinf(a)*sinf(b),r*cosf(a)}));
}
static int render_frame(uint16_t *color,uint16_t *depth,const r3_snapshot_t *s,const r3_world_t *world,r3_camera_follow_t *follow)
{
    if(!color||!depth)return -1;
    for(unsigned i=0;i<R3_VIEW_PIXELS;++i){color[i]=rgb(0x0c1220,1);depth[i]=65535;}
    if(!s||s->version!=ROLL3_CONTRACT_VERSION||!finite_vec(s->position)||!finite_vec(s->velocity)||
       !finite_vec(s->omega)||!finite_vec(s->contact_normal)||!isfinite(s->radius)||s->radius<=0||s->radius>10||
       (unsigned)s->state>R3_ERROR||(unsigned)s->error>R3_INPUT_ERROR||
       r3_world_validate(world)!=R3_OK)return -1;
    float norm=s->orientation.w*s->orientation.w+s->orientation.x*s->orientation.x+
        s->orientation.y*s->orientation.y+s->orientation.z*s->orientation.z;
    if(!isfinite(norm)||fabsf(norm-1)>.05f)return -1;
    r3_quat_t q=s->orientation;float inverse=1/sqrtf(norm);q.w*=inverse;q.x*=inverse;q.y*=inverse;q.z*=inverse;
    r3_vec_t focus=follow?follow_update(follow,s,world):roll3_renderer_focus(s,world);
    static const unsigned faces[12][3]={{0,2,3},{0,3,1},{4,5,7},{4,7,6},{0,1,5},{0,5,4},
        {2,6,7},{2,7,3},{0,4,6},{0,6,2},{1,3,7},{1,7,5}};
    for(unsigned i=0;i<world->box_count;++i){const r3_box_t *b=&world->boxes[i];r3_vec_t v[8];
        for(unsigned j=0;j<8;++j){v[j]=b->center;for(unsigned k=0;k<3;++k){float h=k==0?b->half.x:k==1?b->half.y:b->half.z;
            v[j]=add(v[j],mul(b->axis[k],(j&(1u<<k))?h:-h));}}
        /* Top faces are replaced by coplanar checker cells; no z-fighting overlay. */
        for(unsigned j=0;j<12;++j)if(j!=2&&j!=3)
            triangle(color,depth,v[faces[j][0]],v[faces[j][1]],v[faces[j][2]],b->color,focus);
        unsigned nx=(unsigned)fmaxf(1,fminf(16,ceilf(b->half.x*4)));
        unsigned ny=(unsigned)fmaxf(1,fminf(16,ceilf(b->half.y*4)));
        for(unsigned cy=0;cy<ny;++cy)for(unsigned cx=0;cx<nx;++cx){
            r3_vec_t q[4];
            for(unsigned k=0;k<4;++k){
                float u=-b->half.x+2*b->half.x*(cx+(k==1||k==2))/(float)nx;
                float v0=-b->half.y+2*b->half.y*(cy+(k>=2))/(float)ny;
                q[k]=add(add(add(b->center,mul(b->axis[2],b->half.z)),mul(b->axis[0],u)),mul(b->axis[1],v0));
            }
            uint32_t paint=(cx+cy)%2?b->color:0xf2e6d5;
            triangle(color,depth,q[0],q[1],q[2],paint,focus);
            triangle(color,depth,q[0],q[2],q[3],paint,focus);
        }
    }
    for(unsigned lat=0;lat<6;++lat)for(unsigned lon=0;lon<12;++lon){
        r3_vec_t a=sphere_point(lat,lon,s->radius,q,s->position),b=sphere_point(lat+1,lon,s->radius,q,s->position);
        r3_vec_t c=sphere_point(lat+1,lon+1,s->radius,q,s->position),d=sphere_point(lat,lon+1,s->radius,q,s->position);
        uint32_t paint=(lon/3)%2?0xffe6aa:0xee648b;
        triangle(color,depth,a,b,c,paint,focus);triangle(color,depth,a,c,d,paint,focus);
    }
    return 0;
}
int roll3_renderer_render(uint16_t *color,uint16_t *depth,const r3_snapshot_t *s,const r3_world_t *w)
{return render_frame(color,depth,s,w,NULL);}
int roll3_renderer_render_follow(uint16_t *color,uint16_t *depth,const r3_snapshot_t *s,const r3_world_t *w,r3_camera_follow_t *follow)
{return render_frame(color,depth,s,w,follow);}
int roll3_renderer_selftest(void (*report)(const char *,bool))
{
    float x,y,z,x2,y2,z2;int failures=0;
    r3_vec_t f={0,0,R3_RADIUS};
#define CHECK(name,e) do {bool ok=(e);failures+=!ok;if(report)report(name,ok);}while(0)
    bool center=roll3_renderer_project_focus(f,f,&x,&y,&z);
    CHECK("TC-505 camera-center",center&&fabsf(x-90)<.01f&&fabsf(y-75)<.01f);
    CHECK("TC-505 screen-right",center&&roll3_renderer_project_focus(add(f,(r3_vec_t){1,0,0}),f,&x2,&y2,&z2)&&x2>x);
    CHECK("TC-505 screen-forward",center&&roll3_renderer_project_focus(add(f,(r3_vec_t){0,1,0}),f,&x2,&y2,&z2)&&y2<y);
    CHECK("TC-505 near-guard",!roll3_renderer_project_focus(add(f,(r3_vec_t){0,-3.2f,2.6f}),f,&x2,&y2,&z2));
    CHECK("TC-505 follow-translation",roll3_renderer_project_focus((r3_vec_t){8,5,.7f},(r3_vec_t){8,5,.7f},&x2,&y2,&z2)&&fabsf(x2-x)<.01f&&fabsf(y2-y)<.01f);
    CHECK("TC-505 ball-diameter-24px",center&&2*R3_RADIUS*FOCAL/z*2>=24);
    r3_camera_follow_t c={0};
    r3_world_t world={.spawn=f,.checkpoint_spawn={3.45f,0,.7f}};
    r3_snapshot_t snap={.position=f,.state=R3_RUNNING,.run=1};
    follow_update(&c,&snap,&world);
    bool steady=true;
    for(unsigned i=0;i<20;++i){snap.elapsed_ms+=67;snap.position.x=i%2?.01f:-.01f;
        snap.position.z=f.z+(i%2?.01f:-.01f);follow_update(&c,&snap,&world);
        steady=steady&&c.focus.x==0&&c.focus.z==f.z;}
    CHECK("TC-506 camera-jitter-deadband",steady);
    snap.position=(r3_vec_t){.5f,0,R3_RADIUS};snap.elapsed_ms+=67;
    follow_update(&c,&snap,&world);
    bool smooth=c.focus.x>0&&c.focus.x<.5f;
    for(unsigned i=0;i<20;++i){float before=c.focus.x;snap.elapsed_ms+=67;follow_update(&c,&snap,&world);
        smooth=smooth&&c.focus.x>=before&&c.focus.x<=.5f;}
    CHECK("TC-506 camera-monotonic-follow",smooth&&fabsf(c.focus.x-.5f)<.03f);
    snap.state=R3_FALLEN;snap.checkpoint=1;follow_update(&c,&snap,&world);
    bool reset=c.focus.x==world.checkpoint_spawn.x;
    snap.state=R3_RUNNING;snap.position=f;follow_update(&c,&snap,&world);reset=reset&&c.focus.x==0;
    snap.run++;snap.position.x=2;follow_update(&c,&snap,&world);
    CHECK("TC-506 camera-respawn-new-run",reset&&c.focus.x==2);
#undef CHECK
    return failures;
}
