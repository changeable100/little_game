#ifndef TEST_RTTHREAD_H
#define TEST_RTTHREAD_H
#include <stddef.h>
#include <stdint.h>
typedef int rt_err_t;
typedef void *rt_thread_t;
#define RT_EOK 0
#define RT_IPC_FLAG_FIFO 0
#define RT_NULL NULL
struct rt_messagequeue { uint32_t values[8]; unsigned head, count; };
rt_thread_t rt_thread_self(void);
uint32_t rt_tick_get_millisecond(void);
int rt_kprintf(const char *format, ...);
rt_err_t rt_mq_init(struct rt_messagequeue *, const char *, void *, size_t, size_t, unsigned);
rt_err_t rt_mq_detach(struct rt_messagequeue *);
rt_err_t rt_mq_send(struct rt_messagequeue *, void *, size_t);
rt_err_t rt_mq_recv(struct rt_messagequeue *, void *, size_t, int);
#define MSH_CMD_EXPORT(fn, description) void (*test_export_##fn)(int, char **) = fn
#endif
