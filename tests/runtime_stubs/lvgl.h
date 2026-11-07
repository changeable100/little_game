#ifndef TEST_LVGL_H
#define TEST_LVGL_H
#include <stdint.h>
typedef struct lv_timer_t { void (*callback)(struct lv_timer_t *); } lv_timer_t;
lv_timer_t *lv_timer_create(void (*callback)(lv_timer_t *), uint32_t period, void *data);
uint32_t lv_timer_handler(void);
void lv_timer_delete(lv_timer_t *timer);
#endif
