#ifndef ROLL3_RECORDS_H
#define ROLL3_RECORDS_H
#include "roll3_contract.h"
#include <stddef.h>
#define R3_RECORDS_MAX 10u
#define R3_RECORDS_BYTES 140u
typedef struct { uint32_t elapsed_ms, falls, level; } r3_record_t;
typedef struct {
    r3_record_t rows[R3_RECORDS_MAX];
    uint32_t count, generation, last_run;
} r3_records_t;
typedef struct {
    bool (*read)(unsigned slot, void *data, size_t capacity, size_t *length, void *ctx);
    bool (*write)(unsigned slot, const void *data, size_t length, void *ctx);
    void *ctx;
} r3_records_io_t;
bool r3_records_load(r3_records_t *out, const r3_records_io_t *io);
bool r3_records_observe(r3_records_t *table, const r3_snapshot_t *snapshot);
bool r3_records_observe_level(r3_records_t *table,const r3_snapshot_t *snapshot,unsigned level);
bool r3_records_save(const r3_records_t *table, const r3_records_io_t *io);
int r3_records_selftest(void (*report)(const char *, bool));
/* Service owns locking and background Flash writes. UI receives a copy only. */
typedef enum {R3_RECORDS_OFFLINE, R3_RECORDS_SAVED, R3_RECORDS_PENDING, R3_RECORDS_UNSAVED} r3_records_status_t;
bool roll3_records_init(void);
void roll3_records_observe(const r3_snapshot_t *snapshot);
r3_records_status_t roll3_records_read(r3_records_t *out);
void roll3_records_observe_level(const r3_snapshot_t *snapshot,unsigned level);
r3_records_status_t roll3_records_read_level(unsigned level,r3_records_t *out);
#endif
