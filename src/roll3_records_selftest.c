#include "roll3_records.h"
#include <string.h>
typedef struct {uint8_t p[2][R3_RECORDS_BYTES];size_t n[2];unsigned last,writes;bool fail_read,fail_write,corrupt;} memory_t;
static bool read_memory(unsigned slot,void *out,size_t cap,size_t *n,void *ctx) {
    memory_t *m=ctx;if(m->fail_read)return false;
    *n=m->n[slot];memcpy(out,m->p[slot],*n<cap?*n:cap);return true;
}
static bool write_memory(unsigned slot,const void *data,size_t n,void *ctx) {
    memory_t *m=ctx;if(m->fail_write)return false;
    memcpy(m->p[slot],data,n);m->n[slot]=n;m->last=slot;++m->writes;
    if(m->corrupt)m->p[slot][24]^=1;
    return true;
}
#define CHECK(c) do{if(!(c)){if(report)report("records: " #c,false);return 1;}}while(0)
#define PASS(s) do{if(report)report("TC-531 " s,true);}while(0)
int r3_records_selftest(void (*report)(const char *,bool)) {
    memory_t mem={0};r3_records_io_t io={read_memory,write_memory,&mem};r3_records_t t,restored;
    CHECK(r3_records_load(&t,&io)&&t.count==0);
    r3_snapshot_t s={.version=ROLL3_CONTRACT_VERSION,.run=1,.state=R3_RUNNING,.elapsed_ms=1000};
    CHECK(!r3_records_observe(&t,&s));s.state=R3_ERROR;CHECK(!r3_records_observe(&t,&s));
    s.state=R3_WON;s.elapsed_ms=0;CHECK(!r3_records_observe(&t,&s));
    s.run=2;s.elapsed_ms=2000;s.limited_steps=1;CHECK(!r3_records_observe(&t,&s));
    s.run=3;s.limited_steps=0;s.error=R3_INPUT_ERROR;CHECK(!r3_records_observe(&t,&s));
    s.run=4;s.error=R3_NO_ERROR;s.falls=3;CHECK(r3_records_observe(&t,&s));
    CHECK(!r3_records_observe(&t,&s)&&t.count==1);
    s.state=R3_PAUSED;CHECK(!r3_records_observe(&t,&s));
    PASS("only first valid completed run; no error/zero/limited score");
    s.state=R3_WON;s.run=5;s.elapsed_ms=1000;s.falls=2;CHECK(r3_records_observe(&t,&s));
    s.run=6;s.falls=1;CHECK(r3_records_observe(&t,&s));CHECK(t.rows[0].falls==1&&t.rows[1].falls==2);
    for(unsigned i=7;i<22;++i){s.run=i;s.elapsed_ms=3000+i*100;s.falls=0;(void)r3_records_observe(&t,&s);}
    CHECK(t.count==10&&t.rows[0].elapsed_ms==1000&&t.rows[9].elapsed_ms==4300);
    PASS("time sorting / falls tie-break / top ten cap");
    CHECK(r3_records_save(&t,&io));unsigned first=mem.last;
    CHECK(r3_records_load(&restored,&io)&&restored.count==t.count&&restored.generation==t.generation);
    CHECK(!memcmp(restored.rows,t.rows,sizeof(t.rows))&&restored.last_run==0);
    s.run=1;s.elapsed_ms=900;CHECK(r3_records_observe(&restored,&s));
    s.run=2;s.elapsed_ms=800;CHECK(r3_records_observe(&restored,&s));
    CHECK(r3_records_save(&restored,&io)&&mem.last!=first);
    CHECK(r3_records_load(&t,&io)&&t.rows[0].elapsed_ms==800);
    PASS("restart reload / new boot run / coalesced generations alternate physical slots");
    mem.p[mem.last][30]^=1;CHECK(r3_records_load(&t,&io)&&t.rows[0].elapsed_ms==1000);
    s.run=30;s.elapsed_ms=700;CHECK(r3_records_observe(&t,&s));
    mem.fail_write=true;CHECK(!r3_records_save(&t,&io));mem.fail_write=false;
    CHECK(r3_records_load(&restored,&io)&&restored.rows[0].elapsed_ms==1000);
    mem.corrupt=true;CHECK(!r3_records_save(&t,&io));mem.corrupt=false;
    CHECK(r3_records_load(&restored,&io)&&restored.rows[0].elapsed_ms==1000);
    mem.fail_read=true;CHECK(!r3_records_load(&restored,&io));mem.fail_read=false;
    mem.n[first]=5;CHECK(!r3_records_load(&restored,&io));
    PASS("CRC/truncation recovery / write and read-back failure never claim saved");
    memory_t second={0};r3_records_io_t second_io={read_memory,write_memory,&second};r3_records_t second_table;
    CHECK(r3_records_load(&second_table,&second_io)&&second_table.count==0);
    s.run=1;s.elapsed_ms=1700;s.state=R3_WON;s.error=R3_NO_ERROR;s.limited_steps=0;
    CHECK(r3_records_observe_level(&second_table,&s,2)&&second_table.rows[0].level==2);
    s.run=2;CHECK(!r3_records_observe_level(&second_table,&s,1));
    CHECK(r3_records_save(&second_table,&second_io)&&r3_records_load(&restored,&second_io));
    CHECK(restored.count==1&&restored.rows[0].level==2&&restored.rows[0].elapsed_ms==1700);
    PASS("level2 format reload / reject mixed-level table");
    return 0;
}
