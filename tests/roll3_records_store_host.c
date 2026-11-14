#include "roll3_records.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <string.h>
#include <stdio.h>
static bool online,write_fail,init_allowed;
static unsigned stack_reads,init_calls,retry_calls;
void sifli_nvds_init(void){++init_calls;if(init_allowed)online=true;}
uint8_t sifli_nvds_flash_adaptor_init(void){++retry_calls;if(init_allowed)online=true;return online?0:1;}
static uint8_t data[4][R3_RECORDS_BYTES];static size_t length[4];
struct fdb_kvdb;
struct fdb_kvdb *sifli_nvds_get_ble_kvdb(void){return online?(struct fdb_kvdb *)(uintptr_t)1:NULL;}
static unsigned key_index(const char *key){
 const char *keys[]={"R3_TOP10_A","R3_TOP10_B","R3_L2_TOP10_A","R3_L2_TOP10_B"};
 for(unsigned i=0;i<4;++i)if(!strcmp(key,keys[i]))return i;
 assert(false);return 0;
}
size_t sifli_nvds_flash_read(const char *key,void *p,size_t cap){
 if(!strcmp(key,"SIF_STACK")){++stack_reads;return 0;}
 unsigned i=key_index(key);assert(online);size_t n=length[i]<cap?length[i]:cap;memcpy(p,data[i],n);return n;
}
uint8_t sifli_nvds_flash_write(const char *key,const void *p,size_t n){
 assert(online);if(write_fail)return 1;unsigned i=key_index(key);assert(n<=sizeof(data[i]));memcpy(data[i],p,n);length[i]=n;return 0;
}
extern const r3_records_io_t r3_records_storage_io,r3_records_storage_io_level2;
int main(void){
 r3_records_t a,b;assert(!r3_records_load(&a,&r3_records_storage_io));assert(init_calls==1);
 assert(!r3_records_load(&a,&r3_records_storage_io));assert(init_calls==1&&retry_calls==1);init_allowed=true;
 assert(r3_records_load(&a,&r3_records_storage_io)&&a.count==0);
 r3_snapshot_t s={.version=1,.run=1,.state=R3_WON,.elapsed_ms=12345,.limited_steps=9};
 assert(r3_records_observe(&a,&s));assert(r3_records_save(&a,&r3_records_storage_io));
 assert(r3_records_load(&b,&r3_records_storage_io)&&b.count==1&&b.rows[0].elapsed_ms==12345);
 assert(r3_records_load(&a,&r3_records_storage_io_level2)&&a.count==0);
 assert(r3_records_observe_level(&a,&s,2));write_fail=true;assert(!r3_records_save(&a,&r3_records_storage_io_level2));
 write_fail=false;assert(r3_records_save(&a,&r3_records_storage_io_level2));
 assert(r3_records_load(&b,&r3_records_storage_io_level2)&&b.rows[0].level==2);
 assert(stack_reads==0);puts("TC-536 production storage adapter: DB ready without SIF_STACK / two levels / write failure / reload PASS");return 0;
}
