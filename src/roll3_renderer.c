#include "roll3_renderer.h"
#include "roll3_bend.h"
#include "roll3_camera_basis.h"
#include "roll3_physics.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

#define NEAR_Z .15f
#define FAR_Z 30.0f
#define FOCAL (R3_VIEW_H * .9604911f) /* Vertical FOV 55 degrees. */
static r3_vec_t add(r3_vec_t a,r3_vec_t b){return (r3_vec_t){a.x+b.x,a.y+b.y,a.z+b.z};}
static r3_vec_t sub(r3_vec_t a,r3_vec_t b){return (r3_vec_t){a.x-b.x,a.y-b.y,a.z-b.z};}
static r3_vec_t mul(r3_vec_t a,float s){return (r3_vec_t){a.x*s,a.y*s,a.z*s};}
static float dot(r3_vec_t a,r3_vec_t b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static r3_vec_t cross(r3_vec_t a,r3_vec_t b){return (r3_vec_t){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static bool finite_vec(r3_vec_t p){return isfinite(p.x)&&isfinite(p.y)&&isfinite(p.z);}
static const r3_vec_t eye_offset={-3.2f*R3_CAM_SIN,-3.2f*R3_CAM_COS,2.6f};

static r3_vec_t camera(r3_vec_t p,r3_vec_t focus)
{
    r3_vec_t v=sub(sub(p,focus),eye_offset);
    float length=sqrtf(3.2f*3.2f+2.6f*2.6f);
    float forward=R3_CAM_SIN*v.x+R3_CAM_COS*v.y;
    return (r3_vec_t){R3_CAM_COS*v.x-R3_CAM_SIN*v.y,
        (2.6f*forward+3.2f*v.z)/length,(3.2f*forward-2.6f*v.z)/length};
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
typedef struct {
    uint16_t *color, *depth;
    float sample_x, sample_y;
    const r3_edge_aa_t *edges;
    int first_row, end_row;
    const r3_course_style_t *style;
} r3_raster_t;

enum { EDGE_SAMPLES = 4 };

static bool valid_depth_rows(unsigned rows)
{
    return rows>=2*EDGE_SAMPLES&&rows<=R3_VIEW_H;
}

static unsigned count_bits(uint32_t bits)
{
    bits -= (bits >> 1) & 0x55555555u;
    bits = (bits & 0x33333333u) + ((bits >> 2) & 0x33333333u);
    bits = (bits + (bits >> 4)) & 0x0f0f0f0fu;
    return (bits * 0x01010101u) >> 24;
}

static inline void shade_sample(r3_raster_t *target, unsigned index, float u, float v,
    const float iz[3], uint16_t paint)
{
    float w=1-u-v;
    if(u<-.00001f||v<-.00001f||w<-.00001f)return;
    const float inv_far=1.0f/FAR_Z;
    const float depth_scale=65534.0f/(1.0f/NEAR_Z-inv_far);
    float invz=u*iz[0]+v*iz[1]+w*iz[2];
    uint16_t d=(uint16_t)fmaxf(0,fminf(65534,65534.0f-(invz-inv_far)*depth_scale));
    if(d<target->depth[index]){
        target->depth[index]=d;
        target->color[index]=paint;
    }
}

static void raster(r3_raster_t *target,r3_vec_t a,r3_vec_t b,r3_vec_t c,uint16_t paint)
{
    float x[3],y[3],iz[3];
    r3_vec_t v[3]={a,b,c};
    for(unsigned i=0;i<3;++i){
        iz[i]=1/v[i].z;
        x[i]=R3_VIEW_W*.5f+FOCAL*v[i].x*iz[i];
        y[i]=R3_VIEW_H*.5f-FOCAL*v[i].y*iz[i];
        if(!isfinite(x[i])||!isfinite(y[i]))return;
    }
    float area=edge(x[0],y[0],x[1],y[1],x[2],y[2]);
    if(!isfinite(area)||fabsf(area)<1e-5f)return;
    float minx=fminf(x[0],fminf(x[1],x[2])),maxx=fmaxf(x[0],fmaxf(x[1],x[2]));
    float miny=fminf(y[0],fminf(y[1],y[2])),maxy=fmaxf(y[0],fmaxf(y[1],y[2]));
    if(maxx<0||minx>=R3_VIEW_W||maxy<0||miny>=R3_VIEW_H)return;
    int left=(int)fmaxf(0,floorf(minx)),right=(int)fminf(R3_VIEW_W-1,ceilf(maxx));
    int top=(int)fmaxf(0,floorf(miny)),bottom=(int)fminf(R3_VIEW_H-1,ceilf(maxy));
    if(target->end_row){
        if(top<target->first_row)top=target->first_row;
        if(bottom>=target->end_row)bottom=target->end_row-1;
        if(top>bottom)return;
    }
    float inv_area=1.0f/area;
    float du=(y[2]-y[1])*inv_area,dv=(y[0]-y[2])*inv_area;
    const float ux=.5f*du, vx=.5f*dv;
    const float uy=.5f*(x[1]-x[2])*inv_area, vy=.5f*(x[2]-x[0])*inv_area;
    for(int py=top;py<=bottom;++py){
        float sy=py+target->sample_y,sx=left+target->sample_x;
        float u=edge(x[1],y[1],x[2],y[2],sx,sy)*inv_area;
        float v0=edge(x[2],y[2],x[0],y[0],sx,sy)*inv_area;
        if(!target->edges){
            unsigned index=(unsigned)(py-target->first_row)*R3_VIEW_W+(unsigned)left;
            for(int px=left;px<=right;++px,++index,u+=du,v0+=dv)
                shade_sample(target,index,u,v0,iz,paint);
            continue;
        }

        const uint32_t *mask=target->edges->mask[py];
        const uint16_t *offset=target->edges->offset[py];
        unsigned first_word=(unsigned)left/32u,last_word=(unsigned)right/32u;
        uint32_t left_mask=UINT32_MAX<<((unsigned)left%32u);
        uint32_t right_mask=UINT32_MAX>>(31u-(unsigned)right%32u);
        for(unsigned word=first_word;word<=last_word;++word){
            uint32_t bits=mask[word];
            unsigned index=offset[word];
            if(word==first_word){
                index+=count_bits(bits&~left_mask);
                bits&=left_mask;
            }
            if(word==last_word)bits&=right_mask;
            while(bits){
                unsigned x=word*32u+(unsigned)__builtin_ctz(bits);
                float delta=(float)(x-(unsigned)left);
                float su=u+delta*du,sv=v0+delta*dv;
                unsigned sample=EDGE_SAMPLES*index;
                shade_sample(target,sample,  su,   sv,   iz,paint);
                shade_sample(target,sample+1,su+ux,sv+vx,iz,paint);
                shade_sample(target,sample+2,su+uy,sv+vy,iz,paint);
                shade_sample(target,sample+3,su+ux+uy,sv+vx+vy,iz,paint);
                bits&=bits-1;
                ++index;
            }
        }
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
static void triangle(r3_raster_t *target,r3_vec_t a,r3_vec_t b,r3_vec_t c,uint32_t paint,r3_vec_t focus)
{
    r3_vec_t normal=cross(sub(b,a),sub(c,a));float length=sqrtf(dot(normal,normal));
    if(!isfinite(length)||length<1e-8f)return;
    float light=.35f+.65f*fabsf(dot(mul(normal,1/length),(r3_vec_t){.3f,-.4f,.8660254f}));
    r3_vec_t p[8]={camera(a,focus),camera(b,focus),camera(c,focus)},temp[8];
    if(!finite_vec(p[0])||!finite_vec(p[1])||!finite_vec(p[2]))return;
    unsigned n=clip(p,3,temp,NEAR_Z,true);if(n<3)return;
    n=clip(temp,n,p,FAR_Z,false);if(n<3)return;
    uint16_t shade=rgb(paint,light);
    for(unsigned i=1;i+1<n;++i)raster(target,p[0],p[i],p[i+1],shade);
}
static r3_vec_t rotate(r3_quat_t q,r3_vec_t p)
{
    r3_vec_t u={q.x,q.y,q.z};r3_vec_t t=mul(cross(u,p),2);
    return add(p,add(mul(t,q.w),cross(u,t)));
}
static r3_vec_t sphere_point(unsigned lat,unsigned lon,float r,r3_quat_t q,r3_vec_t pos)
{
    /* Fixed 30-degree mesh: avoid software sin/cos in each band and triangle. */
    static const float sin_lat[]={0,.5f,.8660254f,1,.8660254f,.5f,0};
    static const float cos_lat[]={1,.8660254f,.5f,0,-.5f,-.8660254f,-1};
    static const float sin_lon[]={0,.5f,.8660254f,1,.8660254f,.5f,0,-.5f,-.8660254f,-1,-.8660254f,-.5f};
    static const float cos_lon[]={1,.8660254f,.5f,0,-.5f,-.8660254f,-1,-.8660254f,-.5f,0,.5f,.8660254f};
    unsigned column=lon%12;
    float radial=r*sin_lat[lat];
    return add(pos,rotate(q,(r3_vec_t){radial*cos_lon[column],radial*sin_lon[column],r*cos_lat[lat]}));
}
static void clear_target(r3_raster_t *target)
{
    uint16_t background=rgb(target->style?target->style->background:0x0c1220,1);
    unsigned rows=target->end_row?(unsigned)(target->end_row-target->first_row):R3_VIEW_H;
    for(unsigned i=0;i<rows*R3_VIEW_W;++i){
        target->color[i]=background;
        target->depth[i]=65535;
    }
}

static void bend_quad(r3_raster_t *target, r3_vec_t a, r3_vec_t b, r3_vec_t c, r3_vec_t d,
    uint32_t color, r3_vec_t focus)
{
    triangle(target,a,b,c,color,focus);
    triangle(target,a,c,d,color,focus);
}

static void draw_bend(r3_raster_t *target, const r3_bend_t *bend, r3_vec_t focus,
    const r3_course_style_t *style)
{
    static const float ring[][2]={
        {1.000000000f,0.000000000f},{0.991444861f,0.130526192f},{0.965925826f,0.258819045f},
        {0.923879533f,0.382683432f},{0.866025404f,0.500000000f},{0.793353340f,0.608761429f},
        {0.707106781f,0.707106781f},{0.608761429f,0.793353340f},{0.500000000f,0.866025404f},
        {0.382683432f,0.923879533f},{0.258819045f,0.965925826f},{0.130526192f,0.991444861f},
        {0.000000000f,1.000000000f},{-0.130526192f,0.991444861f},{-0.258819045f,0.965925826f},
        {-0.382683432f,0.923879533f},{-0.500000000f,0.866025404f},{-0.608761429f,0.793353340f},
        {-0.707106781f,0.707106781f},{-0.793353340f,0.608761429f},{-0.866025404f,0.500000000f},
        {-0.923879533f,0.382683432f},{-0.965925826f,0.258819045f},{-0.991444861f,0.130526192f},
        {-1.000000000f,0.000000000f},
    };
    enum { tile_lanes=4, max_lanes=8 };
    const unsigned lanes=bend->trough_depth>0?max_lanes:tile_lanes;
    const unsigned strips_per_tile=lanes/tile_lanes;
    const bool has_rail=bend->rail_height>0;
    const uint32_t rail=style?0xb964d1:0xa071bc;
    const uint32_t light_tile=style?style->light_tile:0xf2e6d5;
    r3_vec_t eye=add(focus,eye_offset);
    for(unsigned i=0;i+1<sizeof(ring)/sizeof(*ring);++i){
        r3_vec_t floor[2][max_lanes+1],bottom[2][2],lip[2],rail_outer[2],rail_base[2];
        for(unsigned end=0;end<2;++end){
            float c=ring[i+end][0],s=ring[i+end][1];
            for(unsigned j=0;j<=lanes;++j){
                float r=bend->radius+bend->half_width*(2.0f*j/lanes-1.0f);
                floor[end][j]=r3_bend_point(bend,c,s,r,r3_bend_height(bend,c,s,r));
            }
            for(unsigned edge=0;edge<2;++edge)
                bottom[end][edge]=sub(floor[end][edge*lanes],(r3_vec_t){0,0,R3_BEND_DEPTH});
            if(style)
                for(unsigned edge=0;edge<2;++edge)bottom[end][edge].z=fminf(bottom[end][edge].z,style->skirt_z);
            if(has_rail){
                lip[end]=add(floor[end][lanes],(r3_vec_t){0,0,bend->rail_height*s*s});
                float outer=bend->radius+bend->half_width+R3_BEND_RAIL_WIDTH;
                rail_outer[end]=r3_bend_point(bend,c,s,outer,lip[end].z);
                rail_base[end]=r3_bend_point(bend,c,s,outer,bottom[end][1].z);
            }
        }
        for(unsigned j=0;j<lanes;++j)
            bend_quad(target,floor[0][j],floor[0][j+1],floor[1][j+1],floor[1][j],
                      (i+j/strips_per_tile)%2?bend->color:light_tile,focus);
        r3_vec_t radial=add(mul(bend->radial_start,ring[i][0]+ring[i+1][0]),
                            mul(bend->sweep_axis,ring[i][1]+ring[i+1][1]));
        if(!style||dot(sub(eye,floor[0][0]),radial)<0)
            bend_quad(target,floor[0][0],floor[1][0],bottom[1][0],bottom[0][0],bend->color,focus);
        if(has_rail){
            if(!style||dot(sub(eye,floor[0][lanes]),radial)<0)
                bend_quad(target,floor[0][lanes],floor[1][lanes],lip[1],lip[0],rail,focus);
            bend_quad(target,lip[0],lip[1],rail_outer[1],rail_outer[0],rail,focus);
            if(!style||dot(sub(eye,rail_outer[0]),radial)>0)
                bend_quad(target,rail_outer[0],rail_outer[1],rail_base[1],rail_base[0],rail,focus);
        }else if(!style||dot(sub(eye,floor[0][lanes]),radial)>0){
            bend_quad(target,floor[0][lanes],floor[1][lanes],bottom[1][1],bottom[0][1],bend->color,focus);
        }
    }
}

static void draw_scene(r3_raster_t *target,const r3_snapshot_t *s,
    const r3_world_t *world,r3_quat_t q,r3_vec_t focus)
{
    static const unsigned faces[12][3]={{0,2,3},{0,3,1},{4,5,7},{4,7,6},{0,1,5},{0,5,4},
        {2,6,7},{2,7,3},{0,4,6},{0,6,2},{1,3,7},{1,7,5}};
    const r3_course_style_t *style=world->style;
    uint32_t light_tile=style?style->light_tile:0xf2e6d5;
    r3_vec_t eye=add(focus,eye_offset);
    for(unsigned i=0;i<world->box_count;++i){const r3_box_t *b=&world->boxes[i];r3_vec_t v[8];
        bool solid=style&&i>=style->solid_boxes_from;
        for(unsigned j=0;j<8;++j){v[j]=b->center;for(unsigned k=0;k<3;++k){float h=k==0?b->half.x:k==1?b->half.y:b->half.z;
            v[j]=add(v[j],mul(b->axis[k],(j&(1u<<k))?h:-h));}}
        if(style&&!solid)
            for(unsigned j=0;j<4;++j)v[j].z=fminf(v[j].z,style->skirt_z);
        /* Road tops use checker cells; rails are drawn as solid boxes. */
        for(unsigned j=0;j<12;j+=2){
            if(!solid&&j==2)continue;
            r3_vec_t a=v[faces[j][0]],q=v[faces[j][1]],c=v[faces[j][2]];
            if(style&&dot(cross(sub(q,a),sub(c,a)),sub(eye,a))<=0)continue;
            triangle(target,a,q,c,b->color,focus);
            triangle(target,v[faces[j+1][0]],v[faces[j+1][1]],v[faces[j+1][2]],b->color,focus);
        }
        if(solid)continue;
        unsigned nx=(unsigned)fmaxf(1,fminf(16,ceilf(b->half.x*4)));
        unsigned ny=(unsigned)fmaxf(1,fminf(16,ceilf(b->half.y*4)));
        for(unsigned cy=0;cy<ny;++cy)for(unsigned cx=0;cx<nx;++cx){
            r3_vec_t q[4];
            for(unsigned k=0;k<4;++k){
                float u=-b->half.x+2*b->half.x*(cx+(k==1||k==2))/(float)nx;
                float v0=-b->half.y+2*b->half.y*(cy+(k>=2))/(float)ny;
                q[k]=add(add(add(b->center,mul(b->axis[2],b->half.z)),mul(b->axis[0],u)),mul(b->axis[1],v0));
            }
            uint32_t paint=(cx+cy)%2?b->color:light_tile;
            triangle(target,q[0],q[1],q[2],paint,focus);
            triangle(target,q[0],q[2],q[3],paint,focus);
        }
    }
    if(world->bend)draw_bend(target,world->bend,focus,style);
    if(style)
        for(unsigned i=0;i<style->face_count;++i){
            const r3_art_face_t *face=&style->faces[i];
            const r3_vec_t *v=face->vertices;
            bend_quad(target,v[0],v[1],v[2],v[3],face->color,focus);
        }
    for(unsigned lat=0;lat<6;++lat)for(unsigned lon=0;lon<12;++lon){
        r3_vec_t a=sphere_point(lat,lon,s->radius,q,s->position),b=sphere_point(lat+1,lon,s->radius,q,s->position);
        r3_vec_t c=sphere_point(lat+1,lon+1,s->radius,q,s->position),d=sphere_point(lat,lon+1,s->radius,q,s->position);
        uint32_t paint=(lon/3)%2?0xffe6aa:0xee648b;
        triangle(target,a,b,c,paint,focus);triangle(target,a,c,d,paint,focus);
    }
}
static uint16_t average_rgb565(uint16_t first,uint16_t second)
{
    unsigned red=((first>>11)+(second>>11)+1u)>>1;
    unsigned green=(((first>>5)&63u)+((second>>5)&63u)+1u)>>1;
    unsigned blue=((first&31u)+(second&31u)+1u)>>1;
    return (uint16_t)((red<<11)|(green<<5)|blue);
}
static bool color_edge(uint16_t a, uint16_t b)
{
    if(a==b)return false;
    int red=(int)(a>>11)-(int)(b>>11);
    int green=(int)((a>>5)&63u)-(int)((b>>5)&63u);
    int blue=(int)(a&31u)-(int)(b&31u);
    /* Approximately twelve levels in RGB888, using native channel steps. */
    return red>=2||red<=-2||green>=3||green<=-3||blue>=2||blue<=-2;
}

static uint16_t mix_play_edge(uint16_t center, uint16_t left, uint16_t right,
    uint16_t above, uint16_t below)
{
    unsigned red=4u*(center>>11)+(left>>11)+(right>>11)+(above>>11)+(below>>11);
    unsigned green=4u*((center>>5)&63u)+((left>>5)&63u)+((right>>5)&63u)+
        ((above>>5)&63u)+((below>>5)&63u);
    unsigned blue=4u*(center&31u)+(left&31u)+(right&31u)+(above&31u)+(below&31u);
    return (uint16_t)((((red+4u)>>3)<<11)|(((green+4u)>>3)<<5)|((blue+4u)>>3));
}

static void smooth_play_edges(uint16_t *color, uint16_t *scratch)
{
    uint16_t *above=scratch,*current=scratch+R3_VIEW_W;
    memcpy(above,color,R3_VIEW_W*sizeof(*color));
    for(unsigned y=0;y<R3_VIEW_H;++y){
        uint16_t *row=color+y*R3_VIEW_W;
        memcpy(current,row,R3_VIEW_W*sizeof(*color));
        const uint16_t *below=y+1<R3_VIEW_H?row+R3_VIEW_W:current;
        for(unsigned x=0;x<R3_VIEW_W;++x){
            uint16_t center=current[x];
            uint16_t left=x?current[x-1]:center,right=x+1<R3_VIEW_W?current[x+1]:center;
            uint16_t top=above[x],bottom=below[x];
            if(center==left&&center==right&&center==top&&center==bottom)continue;
            if(color_edge(center,left)||color_edge(center,right)||
               color_edge(center,top)||color_edge(center,bottom))
                row[x]=mix_play_edge(center,left,right,top,bottom);
        }
        uint16_t *previous=above;
        above=current;
        current=previous;
    }
}

static uint32_t expand_word(uint32_t left, uint32_t center, uint32_t right)
{
    return center | (center<<1) | (center>>1) | (left>>31) | (right<<31);
}

static void mark_edge(uint32_t *row, unsigned x)
{
    row[x/32u]|=UINT32_C(1)<<(x%32u);
}

static void build_edge_mask(const uint16_t *color, r3_edge_aa_t *edges)
{
    memset(edges->mask,0,sizeof(edges->mask));
    for(unsigned y=0;y<R3_VIEW_H;++y){
        const uint16_t *row=color+y*R3_VIEW_W;
        for(unsigned x=0;x<R3_VIEW_W;++x){
            if(x+1<R3_VIEW_W&&color_edge(row[x],row[x+1])){
                mark_edge(edges->mask[y],x);
                mark_edge(edges->mask[y],x+1);
            }
            if(y+1<R3_VIEW_H&&color_edge(row[x],row[x+R3_VIEW_W])){
                mark_edge(edges->mask[y],x);
                mark_edge(edges->mask[y+1],x);
            }
        }
    }

    /* Keep the original previous row while dilating the bitmap in place. */
    uint32_t previous[R3_EDGE_WORDS]={0}, merged[R3_EDGE_WORDS];
    for(unsigned y=0;y<R3_VIEW_H;++y){
        for(unsigned word=0;word<R3_EDGE_WORDS;++word){
            uint32_t center=edges->mask[y][word];
            uint32_t next=y+1<R3_VIEW_H?edges->mask[y+1][word]:0;
            merged[word]=previous[word]|center|next;
            previous[word]=center;
        }
        for(unsigned word=0;word<R3_EDGE_WORDS;++word)
            edges->mask[y][word]=expand_word(word?merged[word-1]:0,merged[word],
                                           word+1<R3_EDGE_WORDS?merged[word+1]:0);
        if(R3_VIEW_W%32u)
            edges->mask[y][R3_EDGE_WORDS-1]&=UINT32_MAX>>(32u-R3_VIEW_W%32u);
    }
}

static uint16_t resolve_four(const uint16_t samples[EDGE_SAMPLES])
{
    unsigned red=0,green=0,blue=0;
    for(unsigned i=0;i<EDGE_SAMPLES;++i){
        red+=samples[i]>>11;
        green+=(samples[i]>>5)&63u;
        blue+=samples[i]&31u;
    }
    return (uint16_t)((((red+2)>>2)<<11)|(((green+2)>>2)<<5)|((blue+2)>>2));
}

static void draw_edge_bands(uint16_t *color, uint16_t *scratch, unsigned depth_rows,
    const r3_snapshot_t *s,
    const r3_world_t *world, r3_quat_t q, r3_vec_t focus, r3_edge_aa_t *edges)
{
    const uint16_t background=rgb(world->style?world->style->background:0x0c1220,1);
    const unsigned half=depth_rows*R3_VIEW_W/2, capacity=half/EDGE_SAMPLES;
    unsigned first=0;
    while(first<R3_VIEW_H){
        unsigned end=first,count=0;
        while(end<R3_VIEW_H){
            unsigned row_count=0;
            for(unsigned word=0;word<R3_EDGE_WORDS;++word)
                row_count+=count_bits(edges->mask[end][word]);
            if(count+row_count>capacity)break;
            for(unsigned word=0;word<R3_EDGE_WORDS;++word){
                edges->offset[end][word]=(uint16_t)count;
                count+=count_bits(edges->mask[end][word]);
            }
            ++end;
        }
        if(count){
            r3_raster_t target={
                .color=scratch+half,.depth=scratch,
                .sample_x=.25f,.sample_y=.25f,.edges=edges,
                .first_row=(int)first,.end_row=(int)end
            };
            for(unsigned i=0;i<count*EDGE_SAMPLES;++i){
                target.color[i]=background;
                target.depth[i]=UINT16_MAX;
            }
            draw_scene(&target,s,world,q,focus);

            unsigned index=0;
            for(unsigned y=first;y<end;++y){
                for(unsigned word=0;word<R3_EDGE_WORDS;++word){
                    uint32_t bits=edges->mask[y][word];
                    while(bits){
                        unsigned x=word*32u+(unsigned)__builtin_ctz(bits);
                        color[y*R3_VIEW_W+x]=resolve_four(target.color+index*EDGE_SAMPLES);
                        bits&=bits-1;
                        ++index;
                    }
                }
            }
        }
        first=end;
    }
}

static int render_frame(uint16_t *color,uint16_t *depth,unsigned depth_rows,const r3_snapshot_t *s,
    const r3_world_t *world,r3_camera_follow_t *follow,uint16_t *aa_color,r3_edge_aa_t *edges)
{
    if(!color||!depth||!valid_depth_rows(depth_rows))return -1;
    bool valid=s&&s->version==ROLL3_CONTRACT_VERSION&&finite_vec(s->position)&&finite_vec(s->velocity)&&
       finite_vec(s->omega)&&finite_vec(s->contact_normal)&&isfinite(s->radius)&&s->radius>0&&s->radius<=10&&
       (unsigned)s->state<=R3_ERROR&&(unsigned)s->error<=R3_INPUT_ERROR&&r3_world_validate(world)==R3_OK;
    float norm=valid?s->orientation.w*s->orientation.w+s->orientation.x*s->orientation.x+
        s->orientation.y*s->orientation.y+s->orientation.z*s->orientation.z:0;
    if(!isfinite(norm)||fabsf(norm-1)>.05f){
        uint16_t background=rgb(0x0c1220,1);
        for(unsigned i=0;i<R3_VIEW_PIXELS;++i)color[i]=background;
        for(unsigned i=0;i<depth_rows*R3_VIEW_W;++i)depth[i]=UINT16_MAX;
        return -1;
    }
    r3_quat_t q=s->orientation;
    float inverse=1/sqrtf(norm);
    q.w*=inverse;q.x*=inverse;q.y*=inverse;q.z*=inverse;
    r3_vec_t focus=follow?follow_update(follow,s,world):roll3_renderer_focus(s,world);
    if(!aa_color){
        for(unsigned first=0;first<R3_VIEW_H;first+=depth_rows){
            unsigned end=first+depth_rows;
            if(end>R3_VIEW_H)end=R3_VIEW_H;
            r3_raster_t target={
                .color=color+first*R3_VIEW_W,.depth=depth,.sample_x=.5f,.sample_y=.5f,
                .first_row=(int)first,.end_row=(int)end,.style=world->style
            };
            clear_target(&target);
            draw_scene(&target,s,world,q,focus);
        }
        if(edges){
            build_edge_mask(color,edges);
            draw_edge_bands(color,depth,depth_rows,s,world,q,focus,edges);
        }
        return 0;
    }

    /* Independent depth tests avoid checker seams and silhouette color bleed. */
    r3_raster_t target={.color=color,.depth=depth,.sample_x=.25f,.sample_y=.75f,.style=world->style};
    clear_target(&target);
    draw_scene(&target,s,world,q,focus);
    target=(r3_raster_t){.color=aa_color,.depth=depth,.sample_x=.75f,.sample_y=.25f,.style=world->style};
    clear_target(&target);
    draw_scene(&target,s,world,q,focus);
    for(unsigned i=0;i<R3_VIEW_PIXELS;++i)
        color[i]=average_rgb565(color[i],aa_color[i]);
    return 0;
}
int roll3_renderer_render(uint16_t *color,uint16_t *depth,const r3_snapshot_t *s,const r3_world_t *w)
{return render_frame(color,depth,R3_VIEW_H,s,w,NULL,NULL,NULL);}
int roll3_renderer_render_follow(uint16_t *color,uint16_t *depth,const r3_snapshot_t *s,const r3_world_t *w,r3_camera_follow_t *follow,uint16_t *aa_color)
{return render_frame(color,depth,R3_VIEW_H,s,w,follow,aa_color,NULL);}
int roll3_renderer_render_edges(uint16_t *color,uint16_t *depth,unsigned depth_rows,const r3_snapshot_t *s,
    const r3_world_t *w,r3_camera_follow_t *follow,r3_edge_aa_t *edges)
{return render_frame(color,depth,depth_rows,s,w,follow,NULL,edges);}
int roll3_renderer_render_play(uint16_t *color,uint16_t *depth,unsigned depth_rows,
    const r3_snapshot_t *s,const r3_world_t *w,r3_camera_follow_t *follow)
{
    int result=render_frame(color,depth,depth_rows,s,w,follow,NULL,NULL);
    if(result==0)smooth_play_edges(color,depth);
    return result;
}

int roll3_renderer_selftest(void (*report)(const char *,bool))
{
    float x,y,z,x2,y2,z2;int failures=0;
    r3_vec_t f={0,0,R3_RADIUS};
#define CHECK(name,e) do {bool ok=(e);failures+=!ok;if(report)report(name,ok);}while(0)
    bool center=roll3_renderer_project_focus(f,f,&x,&y,&z);
    CHECK("TC-505 camera-center",center&&fabsf(x-R3_VIEW_W*.5f)<.01f&&fabsf(y-R3_VIEW_H*.5f)<.01f);
    CHECK("TC-505 screen-right",center&&roll3_renderer_project_focus(add(f,(r3_vec_t){R3_CAM_COS,-R3_CAM_SIN,0}),f,&x2,&y2,&z2)&&x2>x);
    CHECK("TC-505 screen-forward",center&&roll3_renderer_project_focus(add(f,(r3_vec_t){R3_CAM_SIN,R3_CAM_COS,0}),f,&x2,&y2,&z2)&&y2<y);
    CHECK("TC-505 near-guard",!roll3_renderer_project_focus(add(f,eye_offset),f,&x2,&y2,&z2));
    CHECK("TC-505 follow-translation",roll3_renderer_project_focus((r3_vec_t){8,5,.7f},(r3_vec_t){8,5,.7f},&x2,&y2,&z2)&&fabsf(x2-x)<.01f&&fabsf(y2-y)<.01f);
    CHECK("TC-505 ball-diameter-24px",center&&2*R3_RADIUS*FOCAL/z*1.5f>=24);
    CHECK("TC-507 camera-sees-three-cube-faces",
          eye_offset.x<-.5f&&eye_offset.y<-.5f&&eye_offset.z>.5f);
    CHECK("TC-507 antialias-rgb565-resolve",average_rgb565(0xffff,0)==0x8410);
    CHECK("TC-509 edge-threshold-and-word-boundary",
          color_edge(0,2)&&!color_edge(0,1)&&color_edge(0,3u<<5)&&
          expand_word(0x80000000u,0,1)==0x80000001u&&count_bits(0x80000001u)==2);
    CHECK("TC-516 play-edge-weight",
          mix_play_edge(0xffff,0,0,0,0)==0x8410&&
          mix_play_edge(0x34ba,0x34ba,0x34ba,0x34ba,0x34ba)==0x34ba);
    const uint16_t coverage[EDGE_SAMPLES]={0xffff,0xffff,0,0};
    CHECK("TC-509 four-sample-resolve",resolve_four(coverage)==0x8410);
    CHECK("TC-510 bounded-depth-rows",
          !valid_depth_rows(2*EDGE_SAMPLES-1)&&!valid_depth_rows(R3_VIEW_H+1)&&
          valid_depth_rows(R3_VIEW_DEFAULT_DEPTH_ROWS)&&valid_depth_rows(R3_VIEW_LOW_DEPTH_ROWS));
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
