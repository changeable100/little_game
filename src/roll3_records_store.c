/* SDK v2.5.1 synchronous NVDS wraps the existing locked FlashDB instance.
 * Only application keys are used: no new partition, format, reset or BLE tags. */
#include "roll3_records.h"
#include "bf0_ble_common.h"
#include "bf0_sibles_nvds.h"
/* SDK implements this with PKG_USING_FLASHDB; its header prototype is
 * unnecessarily inside CONFIG_BT_MESH. No mesh feature is needed here. */
struct fdb_kvdb;
extern struct fdb_kvdb *sifli_nvds_get_ble_kvdb(void);
static bool init_attempted;
static bool storage_ready(void) {
    if(sifli_nvds_get_ble_kvdb())return true;
    /* The standalone game never calls ble_power_on(). Initialize only NVDS,
     * including its SDK flash mutex, before using the existing database. */
    if(!init_attempted){init_attempted=true;sifli_nvds_init();}
    else (void)sifli_nvds_flash_adaptor_init();
    return sifli_nvds_get_ble_kvdb()!=NULL;
}
static const char *keys[2][2]={{"R3_TOP10_A","R3_TOP10_B"},{"R3_L2_TOP10_A","R3_L2_TOP10_B"}};
static bool read_slot(unsigned slot,void *data,size_t cap,size_t *n,void *ctx) {
    unsigned level=(uintptr_t)ctx==2?1:0;
    if(slot>1||!storage_ready())return false;
    *n=sifli_nvds_flash_read(keys[level][slot],data,cap);return true;
}
static bool write_slot(unsigned slot,const void *data,size_t n,void *ctx) {
    unsigned level=(uintptr_t)ctx==2?1:0;return slot<2&&storage_ready()&&sifli_nvds_flash_write(keys[level][slot],data,n)==NVDS_OK;
}
const r3_records_io_t r3_records_storage_io={read_slot,write_slot,NULL};
const r3_records_io_t r3_records_storage_io_level2={read_slot,write_slot,(void *)(uintptr_t)2};
