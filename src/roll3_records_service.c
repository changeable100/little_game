#include "roll3_records.h"
#include "rtthread.h"
#include <string.h>
extern const r3_records_io_t r3_records_storage_io,r3_records_storage_io_level2;
static const r3_records_io_t *io(unsigned i){return i?&r3_records_storage_io_level2:&r3_records_storage_io;}
static struct rt_mutex lock;
static struct rt_thread thread;
ALIGN(RT_ALIGN_SIZE) static uint8_t stack[3072];
static r3_records_t table[2];
static bool initialized,ready[2],dirty[2],failed[2];
static bool flush_level(unsigned i) {
    r3_records_t copy;
    (void)rt_mutex_take(&lock,RT_WAITING_FOREVER);
    if(!ready[i]){
        (void)rt_mutex_release(&lock);
        r3_records_t restored;
        bool ok=r3_records_load(&restored,io(i));
        (void)rt_mutex_take(&lock,RT_WAITING_FOREVER);
        if(ok&&restored.count&&restored.rows[0].level!=i+1)ok=false;
        if(ok){table[i]=restored;ready[i]=true;failed[i]=false;}
        (void)rt_mutex_release(&lock);return ok;
    }
    if(!dirty[i]){(void)rt_mutex_release(&lock);return true;}
    copy=table[i];(void)rt_mutex_release(&lock);
    bool ok=r3_records_save(&copy,io(i));
    (void)rt_mutex_take(&lock,RT_WAITING_FOREVER);
    failed[i]=!ok;
    if(ok&&table[i].generation==copy.generation)dirty[i]=false;
    (void)rt_mutex_release(&lock);
    rt_kprintf("[roll3:records] level=%u generation=%u save=%s\n",i+1,copy.generation,ok?"PASS":"FAIL");
    return ok;
}
static bool flush_once(void){bool a=flush_level(0),b=flush_level(1);return a&&b;}
static void worker(void *unused) {
    (void)unused;
    for(;;)rt_thread_mdelay(flush_once()?250:2000);
}
bool roll3_records_init(void) {
    if(initialized)return true;
    if(rt_mutex_init(&lock,"r3_rank",RT_IPC_FLAG_PRIO)!=RT_EOK)return false;
    if(rt_thread_init(&thread,"r3_save",worker,NULL,stack,sizeof(stack),RT_MAIN_THREAD_PRIORITY+2,10)!=RT_EOK){rt_mutex_detach(&lock);return false;}
    /* Main initializes before the physics/UI readers exist. Flash runs here
     * only during boot; all later reads/writes belong to the storage worker. */
    for(unsigned i=0;i<2;++i){ready[i]=r3_records_load(&table[i],io(i));if(ready[i]&&table[i].count&&table[i].rows[0].level!=i+1)ready[i]=false;}
    initialized=true;
    if(rt_thread_startup(&thread)!=RT_EOK){initialized=false;rt_thread_detach(&thread);rt_mutex_detach(&lock);return false;}
    return true;
}
void roll3_records_observe(const r3_snapshot_t *s){roll3_records_observe_level(s,1);}
void roll3_records_observe_level(const r3_snapshot_t *s,unsigned level){
    if(!initialized||level<1||level>2)return;unsigned i=level-1;
    (void)rt_mutex_take(&lock,RT_WAITING_FOREVER);
    if(ready[i]&&r3_records_observe_level(&table[i],s,level))dirty[i]=true;
    (void)rt_mutex_release(&lock);
}
r3_records_status_t roll3_records_read(r3_records_t *out){return roll3_records_read_level(1,out);}
r3_records_status_t roll3_records_read_level(unsigned level,r3_records_t *out){
    if(out)memset(out,0,sizeof(*out));
    if(!initialized||level<1||level>2)return R3_RECORDS_OFFLINE;unsigned i=level-1;
    (void)rt_mutex_take(&lock,RT_WAITING_FOREVER);
    if(out)*out=table[i];
    r3_records_status_t status=!ready[i]?R3_RECORDS_OFFLINE:failed[i]?R3_RECORDS_UNSAVED:dirty[i]?R3_RECORDS_PENDING:R3_RECORDS_SAVED;
    (void)rt_mutex_release(&lock);return status;
}
