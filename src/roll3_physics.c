#include "roll3_physics.h"
#include "roll3_bend.h"
#include <float.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

#define SKIN 0.00001f
#define IMPACTS 8u
#define DT 0.005f
#define INV_INERTIA (2.5f/(R3_RADIUS*R3_RADIUS)) /* mass=1kg, I=2mr^2/5 */

static r3_vec_t add(r3_vec_t a,r3_vec_t b) { return (r3_vec_t){a.x+b.x,a.y+b.y,a.z+b.z}; }
static r3_vec_t sub(r3_vec_t a,r3_vec_t b) { return (r3_vec_t){a.x-b.x,a.y-b.y,a.z-b.z}; }
static r3_vec_t mul(r3_vec_t a,float k) { return (r3_vec_t){a.x*k,a.y*k,a.z*k}; }
static float dot(r3_vec_t a,r3_vec_t b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
static r3_vec_t cross(r3_vec_t a,r3_vec_t b) {
    return (r3_vec_t){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
}
static float length(r3_vec_t a) { return sqrtf(dot(a,a)); }
static float clamp(float v,float a,float b) { return v<a?a:v>b?b:v; }
static bool finite_vec(r3_vec_t v) { return isfinite(v.x)&&isfinite(v.y)&&isfinite(v.z); }
static bool bounded(r3_vec_t v) { return finite_vec(v)&&fabsf(v.x)<=100&&fabsf(v.y)<=100&&fabsf(v.z)<=100; }
static uint32_t inc(uint32_t n) { return n==UINT32_MAX?n:n+1; }
static bool cap(r3_vec_t *v,float max) {
    float n=length(*v);
    if(n<=max)return false;
    *v=mul(*v,max/n);return true;
}
static void cap_motion(r3_snapshot_t *s) {
    bool linear=cap(&s->velocity,R3_MAX_SPEED);
    bool angular=cap(&s->omega,R3_MAX_OMEGA);
    s->speed_limited=s->speed_limited||linear||angular;
}
static r3_vec_t local(const r3_box_t *b,r3_vec_t v) {
    return (r3_vec_t){dot(v,b->axis[0]),dot(v,b->axis[1]),dot(v,b->axis[2])};
}
static r3_vec_t world(const r3_box_t *b,r3_vec_t v) {
    return add(add(mul(b->axis[0],v.x),mul(b->axis[1],v.y)),mul(b->axis[2],v.z));
}
static r3_vec_t closest_delta(r3_vec_t p,r3_vec_t h) {
    return sub(p,(r3_vec_t){clamp(p.x,-h.x,h.x),clamp(p.y,-h.y,h.y),clamp(p.z,-h.z,h.z)});
}
/* Outside: exact closest point handles faces, edges and corners. Inside: nearest exit. */
static float separation(const r3_box_t *b,r3_vec_t p,r3_vec_t *normal) {
    r3_vec_t l=local(b,sub(p,b->center)), d=closest_delta(l,b->half);
    float n=length(d);
    if(n>1e-9f) { *normal=world(b,mul(d,1/n)); return n-R3_RADIUS; }
    float pos[3]={l.x,l.y,l.z}, half[3]={b->half.x,b->half.y,b->half.z};
    unsigned axis=0;
    for(unsigned j=1;j<3;j++)if(half[j]-fabsf(pos[j])<half[axis]-fabsf(pos[axis]))axis=j;
    *normal=mul(b->axis[axis],pos[axis]<0?-1.0f:1.0f);
    return -(half[axis]-fabsf(pos[axis]))-R3_RADIUS;
}
typedef enum { BEND_FLOOR, BEND_RAIL, BEND_SURFACES } bend_surface_t;
static const r3_box_t bend_materials[BEND_SURFACES] = {
    [BEND_FLOOR] = {.restitution=.15f,.friction=.6f,.rolling_friction=.08f},
    [BEND_RAIL] = {.restitution=.18f,.friction=.45f,.rolling_friction=.03f}
};

static bool bend_valid(const r3_bend_t *b)
{
    return bounded(b->origin)&&isfinite(b->exit_z)&&fabsf(b->exit_z)<=100&&b->exit_z<=b->origin.z&&
        isfinite(b->radius)&&b->radius<=20&&isfinite(b->half_width)&&b->half_width>R3_RADIUS&&
        b->radius-b->half_width>2*R3_RADIUS&&isfinite(b->bank)&&b->bank>=0&&b->bank<=.5f&&
        isfinite(b->rail_height)&&b->rail_height>=0&&b->rail_height<=1&&b->color<=0xffffffu&&
        isfinite(b->trough_depth)&&b->trough_depth>=0&&b->trough_depth<=.75f&&
        (b->longitudinal_scale==0||(b->longitudinal_scale>=1&&b->longitudinal_scale<=2))&&
        (b->rail_height==0||r3_bend_longitudinal_scale(b)==1)&&
        finite_vec(b->radial_start)&&finite_vec(b->sweep_axis)&&
        fabsf(b->radial_start.z)<.0001f&&fabsf(b->sweep_axis.z)<.0001f&&
        fabsf(dot(b->radial_start,b->radial_start)-1)<.0001f&&
        fabsf(dot(b->sweep_axis,b->sweep_axis)-1)<.0001f&&
        fabsf(dot(b->radial_start,b->sweep_axis))<.0001f;
}

/* Signed gap of the sphere to the continuous floor or outer rail. The
 * straight decks handle the opposite side of the common end line. */
static float bend_gap(const r3_bend_t *b, bend_surface_t surface, r3_vec_t p, r3_vec_t *normal)
{
    if(surface==BEND_RAIL&&b->rail_height==0)return FLT_MAX;
    r3_vec_t offset=sub(p,b->origin);
    float outer=b->radius+b->half_width;
    float bound=outer+R3_BEND_RAIL_WIDTH+R3_RADIUS;
    float scale=r3_bend_longitudinal_scale(b);
    if(fabsf(offset.x)>bound*scale||fabsf(offset.y)>bound*scale)return FLT_MAX;
    float u=dot(offset,b->radial_start),v=dot(offset,b->sweep_axis)/scale;
    float inner=b->radius-b->half_width;
    float radius_squared=u*u+v*v;
    if(v < -SKIN || radius_squared<(inner-R3_RADIUS)*(inner-R3_RADIUS) ||
        radius_squared>bound*bound)return FLT_MAX;

    float plan_radius=sqrtf(radius_squared),cosine=u/plan_radius,sine=fmaxf(v,0)/plan_radius;
    float radial_scale=r3_bend_radial_scale(scale,sine);
    float radius=b->radius+(plan_radius-b->radius)*radial_scale;
    r3_vec_t radial=mul(add(mul(b->radial_start,cosine),mul(b->sweep_axis,sine*scale)),1/radial_scale);
    if(surface==BEND_RAIL){
        float bottom=r3_bend_height(b,cosine,sine,outer)-R3_BEND_DEPTH;
        float top=bottom+R3_BEND_DEPTH+b->rail_height*sine*sine;
        float far_edge=outer+R3_BEND_RAIL_WIDTH;
        float dr=radius-clamp(radius,outer,far_edge),dz=p.z-clamp(p.z,bottom,top);
        float distance=sqrtf(dr*dr+dz*dz);
        if(distance>1e-8f){
            *normal=mul(add(mul(radial,dr),(r3_vec_t){0,0,dz}),1/distance);
            return distance-R3_RADIUS;
        }
        float near=fminf(radius-outer,far_edge-radius);
        float vertical=fminf(p.z-bottom,top-p.z);
        if(near<=vertical)*normal=mul(radial,radius-outer<=far_edge-radius?-1.f:1.f);
        else *normal=(r3_vec_t){0,0,p.z-bottom<=top-p.z?-1.f:1.f};
        return -fminf(near,vertical)-R3_RADIUS;
    }

    float floor_radius=clamp(radius,inner,outer);
    r3_bend_profile_t profile=r3_bend_profile(b,cosine,sine,floor_radius);
    float height=profile.height;
    if(p.z<height-R3_BEND_DEPTH)return FLT_MAX;
    float dr=radius-floor_radius;
    if(dr!=0){
        float dz=fmaxf(0,p.z-height);
        r3_vec_t delta=add(mul(radial,dr),(r3_vec_t){0,0,dz});
        float distance=length(delta);
        *normal=mul(delta,1/distance);
        return distance-R3_RADIUS;
    }
    /* Derivative through the width-preserving ellipse map. */
    float width_rise=(plan_radius-b->radius)*(scale*scale-1)*sine*cosine/radial_scale;
    float angular_grade=(profile.angular_rise+profile.radial_grade*width_rise)/plan_radius;
    float radial_grade=profile.radial_grade*radial_scale;
    float du=cosine*radial_grade-sine*angular_grade;
    float dv=(sine*radial_grade+cosine*angular_grade)/scale;
    r3_vec_t grade=add(mul(b->radial_start,du),mul(b->sweep_axis,dv));
    r3_vec_t n=sub((r3_vec_t){0,0,1},grade);
    *normal=mul(n,1/length(n));
    return (p.z-height)*normal->z-R3_RADIUS;
}

static bool zone_valid(r3_zone_t z) {
    return bounded(z.min)&&bounded(z.max)&&z.min.x<z.max.x&&z.min.y<z.max.y&&z.min.z<z.max.z;
}
static bool inside(r3_zone_t z,r3_vec_t p) {
    return p.x>=z.min.x&&p.x<=z.max.x&&p.y>=z.min.y&&p.y<=z.max.y&&p.z>=z.min.z&&p.z<=z.max.z;
}
r3_result_t r3_world_validate(const r3_world_t *w) {
    if(!w||!w->boxes||!w->box_count||w->box_count>R3_MAX_BOXES||
       !bounded(w->spawn)||!bounded(w->checkpoint_spawn)||!zone_valid(w->checkpoint)||
       !zone_valid(w->goal)||!isfinite(w->fall_z)||fabsf(w->fall_z)>100||
       w->spawn.z<=w->fall_z+R3_RADIUS||w->checkpoint_spawn.z<=w->fall_z+R3_RADIUS||
       !inside(w->checkpoint,w->checkpoint_spawn))return R3_INVALID;
    if(w->goal_box_plus_one>w->box_count)return R3_INVALID;
    if(w->style){
        const r3_course_style_t *style=w->style;
        if(style->face_count>R3_MAX_ART_FACES||(style->face_count&&!style->faces)||
           style->solid_boxes_from>w->box_count||style->background>0xffffffu||
           style->light_tile>0xffffffu||!isfinite(style->skirt_z)||fabsf(style->skirt_z)>100)
            return R3_INVALID;
    }
    if(w->bend){
        if(!bend_valid(w->bend))return R3_INVALID;
        r3_vec_t n;
        for(bend_surface_t i=BEND_FLOOR;i<BEND_SURFACES;++i)
            if(bend_gap(w->bend,i,w->spawn,&n)<-SKIN||
               bend_gap(w->bend,i,w->checkpoint_spawn,&n)<-SKIN)return R3_INVALID;
    }
    for(unsigned i=0;i<w->box_count;i++) {
        const r3_box_t *b=&w->boxes[i];
        if(!bounded(b->center)||!bounded(b->half)||b->half.x<=0||b->half.y<=0||b->half.z<=0||
           !isfinite(b->restitution)||b->restitution<0||b->restitution>1||
           !isfinite(b->friction)||b->friction<0||b->friction>2||
           !isfinite(b->rolling_friction)||b->rolling_friction<0||b->rolling_friction>0.2f||
           b->color>0xffffffu)return R3_INVALID;
        for(unsigned j=0;j<3;j++) {
            if(!finite_vec(b->axis[j])||fabsf(dot(b->axis[j],b->axis[j])-1)>0.0001f)return R3_INVALID;
            for(unsigned k=0;k<j;k++)if(fabsf(dot(b->axis[j],b->axis[k]))>0.0001f)return R3_INVALID;
        }
        if(dot(cross(b->axis[0],b->axis[1]),b->axis[2])<0.9999f)return R3_INVALID;
        r3_vec_t n;
        if(separation(b,w->spawn,&n)<-SKIN||separation(b,w->checkpoint_spawn,&n)<-SKIN)return R3_INVALID;
    }
    return R3_OK;
}
static void reset(r3_game_t *g) {
    uint32_t run=g->s.run, revision=g->s.revision;
    memset(&g->s,0,sizeof(g->s));
    g->s.version=ROLL3_CONTRACT_VERSION;g->s.run=run;g->s.revision=revision;
    g->s.position=g->world->spawn;g->s.radius=R3_RADIUS;g->s.orientation.w=1;
    g->pause_motion_saved=false;
}
r3_result_t r3_init(r3_game_t *g,const r3_world_t *w) {
    if(!g||r3_world_validate(w)!=R3_OK)return R3_INVALID;
    memset(g,0,sizeof(*g));g->world=w;reset(g);g->s.state=R3_READY;return R3_OK;
}
r3_result_t r3_snapshot(const r3_game_t *g,r3_snapshot_t *out) {
    if(!g||!g->world||!out)return R3_INVALID;
    *out=g->s;return R3_OK;
}
r3_result_t r3_action(r3_game_t *g,const r3_action_t *a) {
    if(!g||!g->world||!a||!a->request||a->kind<R3_START||a->kind>R3_RESPAWN)return R3_INVALID;
    if(a->expected_run!=g->s.run||a->request<=g->last_request)return R3_STALE;
    if(g->s.revision==UINT32_MAX)return R3_EXHAUSTED;
    r3_state_t next;bool fresh=false;
    switch(a->kind) {
    case R3_START:if(g->s.state!=R3_READY)return R3_WRONG_STATE;next=R3_CALIBRATING;fresh=true;break;
    case R3_RESTART:if(g->s.state==R3_READY)return R3_WRONG_STATE;next=R3_CALIBRATING;fresh=true;break;
    case R3_BACK:next=R3_READY;fresh=true;break;
    case R3_PAUSE:if(g->s.state!=R3_RUNNING)return R3_WRONG_STATE;next=R3_PAUSED;break;
    case R3_RESUME:if(g->s.state!=R3_PAUSED)return R3_WRONG_STATE;next=R3_RUNNING;break;
    case R3_RESPAWN:if(g->s.state!=R3_FALLEN&&g->s.state!=R3_PAUSED)return R3_WRONG_STATE;next=R3_RUNNING;break;
    default:return R3_INVALID;
    }
    if(fresh) {
        if(g->s.run==UINT32_MAX)return R3_EXHAUSTED;
        ++g->s.run;reset(g);
    }
    if(a->kind==R3_RESPAWN) {
        g->s.position=g->s.checkpoint?g->world->checkpoint_spawn:g->world->spawn;
        g->s.orientation=(r3_quat_t){1,0,0,0};g->s.grounded=false;g->s.contacts=0;
        g->s.contact_normal=(r3_vec_t){0,0,0};
        g->pause_motion_saved=false;
    }
    if(a->kind==R3_PAUSE){g->resume_velocity=g->s.velocity;g->resume_omega=g->s.omega;g->pause_motion_saved=true;}
    if(a->kind==R3_RESUME&&g->pause_motion_saved){
        g->s.velocity=g->resume_velocity;g->s.omega=g->resume_omega;g->pause_motion_saved=false;
    }
    if(next!=R3_RUNNING||a->kind==R3_RESPAWN)g->s.velocity=g->s.omega=(r3_vec_t){0,0,0};
    g->s.state=next;g->last_request=a->request;++g->s.revision;return R3_OK;
}
r3_result_t r3_calibrated(r3_game_t *g,uint32_t run) {
    if(!g||!g->world)return R3_INVALID;
    if(run!=g->s.run)return R3_STALE;
    if(g->s.state!=R3_CALIBRATING)return R3_WRONG_STATE;
    if(g->s.revision==UINT32_MAX)return R3_EXHAUSTED;
    g->s.state=R3_RUNNING;++g->s.revision;return R3_OK;
}

r3_result_t r3_input_failed(r3_game_t *g, uint32_t run)
{
    if (!g || !g->world) return R3_INVALID;
    if (run != g->s.run) return R3_STALE;
    if (g->s.state != R3_CALIBRATING && g->s.state != R3_RUNNING &&
        g->s.state != R3_PAUSED && g->s.state != R3_FALLEN) return R3_WRONG_STATE;
    if (g->s.revision == UINT32_MAX) return R3_EXHAUSTED;
    g->s.state = R3_ERROR;
    g->s.error = R3_INPUT_ERROR;
    g->s.velocity = g->s.omega = (r3_vec_t){0, 0, 0};
    ++g->s.revision;
    return R3_OK;
}

typedef struct { bool found; float t; r3_vec_t normal; const r3_box_t *material; } hit_t;
/* Inflated AABB is only a broad phase; narrow phase is the rounded box. */
static bool broad(r3_vec_t p,r3_vec_t d,r3_vec_t half) {
    float a[3]={p.x,p.y,p.z},v[3]={d.x,d.y,d.z},h[3]={half.x+R3_RADIUS,half.y+R3_RADIUS,half.z+R3_RADIUS};
    float lo=0,hi=1;
    for(unsigned i=0;i<3;i++) {
        if(fabsf(v[i])<1e-10f) { if(a[i]<-h[i]||a[i]>h[i])return false; }
        else {
            float t0=(-h[i]-a[i])/v[i],t1=(h[i]-a[i])/v[i];
            if(t0>t1) {float temp=t0;t0=t1;t1=temp;}
            if(t0>lo)lo=t0;if(t1<hi)hi=t1;if(lo>hi)return false;
        }
    }
    return true;
}
static void candidate(hit_t *hit,const r3_box_t *b,r3_vec_t p,r3_vec_t d,float t) {
    if(t < -0.00001f||t>1|| (hit->found&&t>=hit->t))return;
    t=t<0?0:t;
    r3_vec_t delta=closest_delta(add(p,mul(d,t)),b->half);
    float dist=length(delta);
    if(dist<1e-9f||fabsf(dist-R3_RADIUS)>0.0002f)return;
    r3_vec_t n=mul(delta,1/dist);
    if(dot(d,n)>=-1e-8f)return;
    hit->found=true;hit->t=t;hit->normal=world(b,n);hit->material=b;
}
static void root_candidate(hit_t *hit,const r3_box_t *b,r3_vec_t p,r3_vec_t d,float aa,float bb,float cc) {
    if(aa<1e-14f)return;
    float discriminant=bb*bb-4*aa*cc;
    if(discriminant<0)return;
    candidate(hit,b,p,d,(-bb-sqrtf(discriminant))/(2*aa));
}
static void sweep_box(hit_t *hit,const r3_box_t *b,r3_vec_t start,r3_vec_t delta) {
    r3_vec_t p=local(b,sub(start,b->center)),d=local(b,delta);
    if(!broad(p,d,b->half))return;
    float pos[3]={p.x,p.y,p.z},dir[3]={d.x,d.y,d.z},half[3]={b->half.x,b->half.y,b->half.z};
    for(unsigned i=0;i<3;i++)for(int sign=-1;sign<=1;sign+=2) {
        if(fabsf(dir[i])>1e-10f)candidate(hit,b,p,d,(sign*(half[i]+R3_RADIUS)-pos[i])/dir[i]);
    }
    for(unsigned free_axis=0;free_axis<3;free_axis++) {
        unsigned a=(free_axis+1)%3,baxis=(free_axis+2)%3;
        for(int sa=-1;sa<=1;sa+=2)for(int sb=-1;sb<=1;sb+=2) {
            float u=pos[a]-sa*half[a],v=pos[baxis]-sb*half[baxis];
            root_candidate(hit,b,p,d,dir[a]*dir[a]+dir[baxis]*dir[baxis],
                2*(u*dir[a]+v*dir[baxis]),u*u+v*v-R3_RADIUS*R3_RADIUS);
        }
    }
    for(int sx=-1;sx<=1;sx+=2)for(int sy=-1;sy<=1;sy+=2)for(int sz=-1;sz<=1;sz+=2) {
        r3_vec_t offset=sub(p,(r3_vec_t){sx*half[0],sy*half[1],sz*half[2]});
        root_candidate(hit,b,p,d,dot(d,d),2*dot(offset,d),dot(offset,offset)-R3_RADIUS*R3_RADIUS);
    }
}
/* Shallow persistent support is integrated on the smooth manifold, then
 * projected by depenetrate. Subdivide its travel (not the collision budget)
 * until endpoint correction is <=1mm and the normal changes by <6 degrees.
 * Normal impacts, lips, and large penetrations retain continuous sweeps. */
static bool bend_support_move(const r3_bend_t *bend,r3_vec_t start,r3_vec_t velocity,float *duration)
{
    r3_vec_t n;
    float gap=bend_gap(bend,BEND_FLOOR,start,&n);
    if(gap < -SKIN||gap>.001f||n.z<=.25f||fabsf(dot(velocity,n))>.15f)return false;
    float span=*duration;
    for(unsigned split=0;split<5;++split){
        r3_vec_t end_normal;
        float end_gap=bend_gap(bend,BEND_FLOOR,add(start,mul(velocity,span)),&end_normal);
        if(end_gap>0)return false;
        if(end_gap>=-.001f&&dot(n,end_normal)>.995f){*duration=span;return true;}
        span*=.5f;
    }
    return false;
}

static void sweep_bend(hit_t *hit, const r3_bend_t *bend, bend_surface_t surface,
    r3_vec_t start, r3_vec_t delta)
{
    r3_vec_t normal;
    float high=hit->found?hit->t:1;
    if(bend_gap(bend,surface,add(start,mul(delta,high)),&normal)>0)return;
    float low=0;
    if(bend_gap(bend,surface,start,&normal)<=0)high=0;
    for(unsigned step=0;step<12&&high>low;++step){
        float mid=(low+high)*.5f;
        if(bend_gap(bend,surface,add(start,mul(delta,mid)),&normal)<=0)high=mid;
        else low=mid;
    }
    if(hit->found&&high>=hit->t)return;
    if(bend_gap(bend,surface,add(start,mul(delta,high)),&normal)==FLT_MAX||
        dot(delta,normal)>=-1e-8f)return;
    *hit=(hit_t){true,high,normal,&bend_materials[surface]};
}

static void mark_contact(r3_game_t *g,const r3_box_t *box,r3_vec_t n) {
    r3_snapshot_t *s=&g->s;
    if(s->contacts<UINT8_MAX)++s->contacts;
    if(n.z>0.25f)s->grounded=true;
    s->contact_normal=n;
    if(g->world->goal_box_plus_one&&box==&g->world->boxes[g->world->goal_box_plus_one-1]&&
       n.z>.25f&&dot(n,box->axis[2])>.25f&&
       dot(sub(s->position,box->center),box->axis[2])>=box->half.z-SKIN)
        g->goal_touched=true;
}
static void impulse(r3_game_t *g,const r3_box_t *box,r3_vec_t n) {
    r3_snapshot_t *s=&g->s;
    mark_contact(g,box,n);
    float vn=dot(s->velocity,n);
    if(vn>=0)return;
    float e=vn < -0.5f ? box->restitution : 0;
    float jn=-(1+e)*vn;
    s->velocity=add(s->velocity,mul(n,jn));
    r3_vec_t arm=mul(n,-R3_RADIUS);
    r3_vec_t contact=add(s->velocity,cross(s->omega,arm));
    r3_vec_t tangent=sub(contact,mul(n,dot(contact,n)));
    float slip=length(tangent);
    if(slip>1e-9f) {
        float jt=fminf(slip/3.5f,box->friction*jn);
        r3_vec_t friction=mul(tangent,-jt/slip);
        s->velocity=add(s->velocity,friction);
        s->omega=add(s->omega,mul(cross(arm,friction),INV_INERTIA));
    }
    r3_vec_t roll=sub(s->omega,mul(n,dot(s->omega,n)));
    float spin=length(roll);
    if(spin>1e-9f) {
        float reduction=fminf(spin,box->rolling_friction*jn*R3_RADIUS*INV_INERTIA);
        s->omega=sub(s->omega,mul(roll,reduction/spin));
    }
    cap_motion(s);
}
static void limited(r3_snapshot_t *s) {
    if(!s->limited)s->limited_steps=inc(s->limited_steps);
    s->limited=true;s->velocity=s->omega=(r3_vec_t){0,0,0};
}
static void depenetrate(r3_game_t *g) {
    for(unsigned pass=0;pass<IMPACTS;pass++) {
        bool corrected=false;
        for(unsigned i=0;i<g->world->box_count;i++) {
            r3_vec_t n;float gap=separation(&g->world->boxes[i],g->s.position,&n);
            if(gap < -SKIN) {
                g->s.position=add(g->s.position,mul(n,SKIN-gap));
                impulse(g,&g->world->boxes[i],n);corrected=true;
            } else if(gap<=2*SKIN)mark_contact(g,&g->world->boxes[i],n);
        }
        if(g->world->bend){
            for(bend_surface_t i=BEND_FLOOR;i<BEND_SURFACES;++i){
                r3_vec_t n;
                float gap=bend_gap(g->world->bend,i,g->s.position,&n);
                if(gap < -SKIN){
                    g->s.position=add(g->s.position,mul(n,SKIN-gap));
                    impulse(g,&bend_materials[i],n);corrected=true;
                }else if(gap<=2*SKIN)mark_contact(g,&bend_materials[i],n);
            }
        }
        if(!corrected)return;
    }
    limited(&g->s);
}
static void orientation(r3_snapshot_t *s) {
    r3_quat_t q=s->orientation;
    r3_vec_t a=s->omega;
    r3_quat_t o={q.w-DT*0.5f*(a.x*q.x+a.y*q.y+a.z*q.z),
        q.x+DT*0.5f*(a.x*q.w+a.y*q.z-a.z*q.y),
        q.y+DT*0.5f*(-a.x*q.z+a.y*q.w+a.z*q.x),
        q.z+DT*0.5f*(a.x*q.y-a.y*q.x+a.z*q.w)};
    float norm=sqrtf(o.w*o.w+o.x*o.x+o.y*o.y+o.z*o.z);
    s->orientation=(r3_quat_t){o.w/norm,o.x/norm,o.y/norm,o.z/norm};
}
r3_result_t r3_step(r3_game_t *g,const r3_input_t *input,uint32_t dt_ms) {
    if(!g||!g->world||!input||dt_ms!=R3_STEP_MS)return R3_INVALID;
    bool completed=g->s.state==R3_WON&&g->world->finish_rolls;
    if(g->s.state!=R3_RUNNING&&!completed)return R3_WRONG_STATE;
    if(g->s.revision==UINT32_MAX)return R3_EXHAUSTED;
    if(!input->valid||!finite_vec(input->acceleration)||dot(input->acceleration,input->acceleration)>400) {
        (void)r3_input_failed(g, g->s.run);
        return R3_INVALID;
    }
    g->s.contacts=0;g->s.grounded=false;g->s.limited=false;g->s.speed_limited=false;
    g->goal_touched=false;
    g->s.contact_normal=(r3_vec_t){0,0,0};
    g->s.velocity=add(g->s.velocity,mul(input->acceleration,DT));
    cap_motion(&g->s);
    depenetrate(g);
    float remaining=g->goal_touched&&!g->world->finish_rolls?0:DT;
    for(unsigned impact=0;impact<IMPACTS&&remaining>1e-8f&&(!g->goal_touched||g->world->finish_rolls);impact++) {
        float span=remaining;
        bool support=g->world->bend&&bend_support_move(g->world->bend,g->s.position,g->s.velocity,&span);
        r3_vec_t delta=mul(g->s.velocity,span);
        hit_t hit={false,1,{0,0,0},0};
        for(unsigned i=0;i<g->world->box_count;i++)sweep_box(&hit,&g->world->boxes[i],g->s.position,delta);
        if(g->world->bend)
            for(bend_surface_t i=BEND_FLOOR;i<BEND_SURFACES;++i)
                if(i!=BEND_FLOOR||!support)sweep_bend(&hit,g->world->bend,i,g->s.position,delta);
        if(!hit.found) {
            g->s.position=add(g->s.position,delta);remaining-=span;
            if(remaining>1e-8f)depenetrate(g);
            continue;
        }
        g->s.position=add(add(g->s.position,mul(delta,hit.t)),mul(hit.normal,SKIN));
        remaining-=span*hit.t;impulse(g,hit.material,hit.normal);
    }
    if(g->goal_touched&&!g->world->finish_rolls)remaining=0;
    if(remaining>1e-8f)limited(&g->s);
    depenetrate(g);cap_motion(&g->s);orientation(&g->s);
    if(!completed)g->s.elapsed_ms=UINT32_MAX-g->s.elapsed_ms<R3_STEP_MS?UINT32_MAX:g->s.elapsed_ms+R3_STEP_MS;
    if(g->s.grounded&&inside(g->world->checkpoint,g->s.position))g->s.checkpoint=1;
    if(!completed&&g->s.position.z<g->world->fall_z) {g->s.state=R3_FALLEN;g->s.falls=inc(g->s.falls);}
    else if(!completed&&(g->world->goal_box_plus_one?g->goal_touched:
        (g->s.grounded&&inside(g->world->goal,g->s.position))))g->s.state=R3_WON;
    if(g->s.state!=R3_RUNNING&&!(g->s.state==R3_WON&&g->world->finish_rolls))g->s.velocity=g->s.omega=(r3_vec_t){0,0,0};
    ++g->s.revision;return R3_OK;
}
