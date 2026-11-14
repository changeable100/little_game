#include "roll3_records.h"
#include <string.h>

static void put(uint8_t *p,uint32_t n) { for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(n>>(8*i)); }
static uint32_t get(const uint8_t *p) { return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static uint32_t crc(const uint8_t *p,size_t n) {
    uint32_t c=UINT32_MAX;
    while(n--){c^=*p++;for(unsigned i=0;i<8;++i)c=(c>>1)^((0u-(c&1u))&UINT32_C(0xedb88320));}
    return ~c;
}
static bool before(r3_record_t a,r3_record_t b) {
    return a.elapsed_ms<b.elapsed_ms||(a.elapsed_ms==b.elapsed_ms&&a.falls<b.falls);
}
static void encode(const r3_records_t *t,uint8_t *p) {
    memset(p,0,R3_RECORDS_BYTES);memcpy(p,"R3R1",4);
    put(p+4,1);put(p+8,t->generation);put(p+12,t->count);
    for(unsigned i=0;i<t->count;++i){put(p+16+i*12,t->rows[i].elapsed_ms);put(p+20+i*12,t->rows[i].falls);put(p+24+i*12,t->rows[i].level);}
    put(p+136,crc(p,136));
}
static bool decode(r3_records_t *t,const uint8_t *p,size_t n) {
    if(n!=R3_RECORDS_BYTES||memcmp(p,"R3R1",4)||get(p+4)!=1||get(p+136)!=crc(p,136)||get(p+12)>R3_RECORDS_MAX)return false;
    memset(t,0,sizeof(*t));t->generation=get(p+8);t->count=get(p+12);
    if(!t->generation||!t->count)return false;
    for(unsigned i=0;i<t->count;++i){
        t->rows[i]=(r3_record_t){get(p+16+i*12),get(p+20+i*12),get(p+24+i*12)};
        if(!t->rows[i].elapsed_ms||t->rows[i].level<1||t->rows[i].level>2||(i&&(t->rows[i].level!=t->rows[0].level||before(t->rows[i],t->rows[i-1]))))return false;
    }
    return true;
}
bool r3_records_load(r3_records_t *out,const r3_records_io_t *io) {
    if(!out||!io||!io->read)return false;
    r3_records_t best={0},candidate;bool valid=false,occupied=false;
    for(unsigned s=0;s<2;++s){
        uint8_t p[R3_RECORDS_BYTES];size_t n=0;
        if(!io->read(s,p,sizeof(p),&n,io->ctx))return false;
        occupied|=n!=0;
        if(decode(&candidate,p,n)&&(!valid||candidate.generation>best.generation)){best=candidate;valid=true;}
    }
    if(occupied&&!valid)return false; /* Never silently replace damaged records. */
    *out=best;return true;
}
bool r3_records_observe(r3_records_t *t,const r3_snapshot_t *s) {
    return r3_records_observe_level(t,s,1);
}
static bool insert(r3_records_t *t,r3_record_t row) {
    if(t->generation==UINT32_MAX)return false;
    unsigned at=0;
    while(at<t->count&&!before(row,t->rows[at]))++at;
    if(at==R3_RECORDS_MAX)return false;
    unsigned count=t->count<R3_RECORDS_MAX?t->count+1:R3_RECORDS_MAX;
    for(unsigned i=count-1;i>at;--i)t->rows[i]=t->rows[i-1];
    t->rows[at]=row;t->count=count;++t->generation;return true;
}
bool r3_records_observe_level(r3_records_t *t,const r3_snapshot_t *s,unsigned level) {
    if(level<1||level>2||(t&&t->count&&t->rows[0].level!=level))return false;
    if(!t||!s||s->version!=ROLL3_CONTRACT_VERSION||s->state!=R3_WON||!s->run||s->run<=t->last_run)return false;
    t->last_run=s->run;
    /* Budget counters diagnose contact resolution, not an invalid completion.
     * The authoritative WON snapshot already owns the frozen score. */
    if(s->error!=R3_NO_ERROR||!s->elapsed_ms)return false;
    return insert(t,(r3_record_t){s->elapsed_ms,s->falls,level});
}
bool r3_records_merge_pending(r3_records_t *restored,const r3_records_t *pending) {
    if(!restored||!pending||pending->count>R3_RECORDS_MAX||restored->count>R3_RECORDS_MAX)return false;
    if(restored->count&&pending->count&&restored->rows[0].level!=pending->rows[0].level)return false;
    bool changed=false;
    for(unsigned i=0;i<pending->count;++i)changed=insert(restored,pending->rows[i])||changed;
    restored->last_run=pending->last_run;
    return changed;
}
bool r3_records_save(const r3_records_t *t,const r3_records_io_t *io) {
    if(!t||!t->count||t->count>R3_RECORDS_MAX||!t->generation||!io||!io->write||!io->read)return false;
    uint8_t p[R3_RECORDS_BYTES],verify[R3_RECORDS_BYTES];size_t n=0;
    /* Several wins may coalesce before the worker runs. Choose the inactive
     * physical slot, rather than assuming generation parity still alternates. */
    unsigned slot=0;uint32_t latest=0;bool found=false;
    for(unsigned i=0;i<2;++i){
        r3_records_t old;
        if(!io->read(i,verify,sizeof(verify),&n,io->ctx))return false;
        if(decode(&old,verify,n)&&(!found||old.generation>latest)){found=true;latest=old.generation;slot=1-i;}
    }
    encode(t,p);
    return io->write(slot,p,sizeof(p),io->ctx)&&io->read(slot,verify,sizeof(verify),&n,io->ctx)&&n==sizeof(p)&&!memcmp(p,verify,n);
}
