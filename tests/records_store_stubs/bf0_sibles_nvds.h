#include <stddef.h>
#include <stdint.h>
size_t sifli_nvds_flash_read(const char *,void *,size_t);
uint8_t sifli_nvds_flash_write(const char *,const void *,size_t);
void sifli_nvds_init(void);
uint8_t sifli_nvds_flash_adaptor_init(void);
