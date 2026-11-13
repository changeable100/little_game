#include "roll3_records.h"
#include "rtthread.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
static uint8_t blobs[2][2][R3_RECORDS_BYTES];static size_t lengths[2][2];
static bool background,write_error,concurrent;static unsigned writes;
static bool read_slot(unsigned slot,void *p,size_t cap,size_t *n,void *ctx){unsigned level=(uintptr_t)ctx==2?1:0;*n=lengths[level][slot];assert(*n<=cap);memcpy(p,blobs[level][slot],*n);return true;}
static bool write_slot(unsigned slot,const void *p,size_t n,void *ctx){
    unsigned level=(uintptr_t)ctx==2?1:0;assert(background);++writes;if(write_error)return false;
    memcpy(blobs[level][slot],p,n);lengths[level][slot]=n;
    if(concurrent){concurrent=false;r3_snapshot_t s={.version=1,.run=2,.state=R3_WON,.elapsed_ms=500};roll3_records_observe(&s);}
    return true;
}
const r3_records_io_t r3_records_storage_io={read_slot,write_slot,NULL};
const r3_records_io_t r3_records_storage_io_level2={read_slot,write_slot,(void*)(uintptr_t)2};
rt_err_t rt_mutex_init(struct rt_mutex *m,const char *s,unsigned flags){(void)s;(void)flags;m->held=0;return 0;}
rt_err_t rt_mutex_take(struct rt_mutex *m,int timeout){(void)timeout;assert(!m->held);m->held=1;return 0;}
rt_err_t rt_mutex_release(struct rt_mutex *m){assert(m->held);m->held=0;return 0;}
rt_err_t rt_mutex_detach(struct rt_mutex *m){assert(!m->held);return 0;}
rt_err_t rt_thread_init(struct rt_thread *t,const char *s,void(*f)(void*),void *arg,void *stack,size_t n,unsigned priority,unsigned slice){
    (void)s;(void)f;(void)arg;(void)stack;(void)n;(void)slice;assert(priority>RT_MAIN_THREAD_PRIORITY);t->initialized=1;return 0;
}
rt_err_t rt_thread_startup(struct rt_thread *t){assert(t->initialized);return 0;}
rt_err_t rt_thread_detach(struct rt_thread *t){t->initialized=0;return 0;}
void rt_thread_mdelay(int n){(void)n;assert(false);}
int rt_kprintf(const char *s,...){(void)s;return 0;}
#include "../src/roll3_records_service.c"
int main(void){
    r3_records_t t;assert(roll3_records_read(&t)==R3_RECORDS_OFFLINE);
    assert(roll3_records_init()&&roll3_records_init());
    r3_snapshot_t s={.version=1,.run=1,.state=R3_WON,.elapsed_ms=1000};
    roll3_records_observe(&s);assert(writes==0&&roll3_records_read(&t)==R3_RECORDS_PENDING);
    background=true;write_error=true;assert(!flush_once());assert(roll3_records_read(&t)==R3_RECORDS_UNSAVED);
    write_error=false;concurrent=true;assert(flush_once());assert(roll3_records_read(&t)==R3_RECORDS_PENDING&&t.rows[0].elapsed_ms==500);
    assert(flush_once());assert(roll3_records_read(&t)==R3_RECORDS_SAVED);
    assert(r3_records_load(&t,&r3_records_storage_io)&&t.rows[0].elapsed_ms==500&&t.count==2);
    s.run=3;s.elapsed_ms=750;roll3_records_observe_level(&s,2);assert(roll3_records_read_level(2,&t)==R3_RECORDS_PENDING&&t.count==1&&t.rows[0].level==2);assert(flush_once());
    assert(r3_records_load(&t,&r3_records_storage_io_level2)&&t.count==1&&t.rows[0].elapsed_ms==750&&t.rows[0].level==2);
    assert(r3_records_load(&t,&r3_records_storage_io)&&t.count==2&&t.rows[0].elapsed_ms==500&&t.rows[0].level==1);
    puts("TC-535 separate level tables and flash reload with original level1 format PASS");
    puts("TC-531 background-only storage, pending/error/saved, concurrent score retention PASS");return 0;
}
