/* SDK v2.5.1 synchronous NVDS wraps the existing locked FlashDB instance.
 * Only application keys are used: no new partition, format, reset or BLE tags. */
#include "roll3_records.h"
#include "bf0_ble_common.h"
#include "bf0_sibles_nvds.h"
static const char *keys[2][2]={{"R3_TOP10_A","R3_TOP10_B"},{"R3_L2_TOP10_A","R3_L2_TOP10_B"}};
static bool read_slot(unsigned slot,void *data,size_t cap,size_t *n,void *ctx) {
    unsigned level=(uintptr_t)ctx==2?1:0;uint8_t ready;
    if(slot>1||!sifli_nvds_flash_read("SIF_STACK",&ready,1))return false;
    *n=sifli_nvds_flash_read(keys[level][slot],data,cap);return true;
}
static bool write_slot(unsigned slot,const void *data,size_t n,void *ctx) {
    unsigned level=(uintptr_t)ctx==2?1:0;return slot<2&&sifli_nvds_flash_write(keys[level][slot],data,n)==NVDS_OK;
}
const r3_records_io_t r3_records_storage_io={read_slot,write_slot,NULL};
const r3_records_io_t r3_records_storage_io_level2={read_slot,write_slot,(void *)(uintptr_t)2};
