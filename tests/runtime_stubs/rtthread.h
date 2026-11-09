#ifndef TEST_RTTHREAD_H
#define TEST_RTTHREAD_H
#include <stddef.h>
#include <stdint.h>
typedef int rt_err_t;
typedef void *rt_thread_t;
#define RT_EOK 0
#define RT_IPC_FLAG_FIFO 0
#define RT_IPC_FLAG_PRIO 1
#define RT_WAITING_FOREVER (-1)
#define RT_MAIN_THREAD_PRIORITY 19
#define RT_ALIGN_SIZE 4
#define ALIGN(n) __attribute__((aligned(n)))
#define RT_NULL NULL
struct rt_messagequeue { uint32_t values[8]; unsigned head, count; };
struct rt_mutex { unsigned held; };
struct rt_thread { unsigned initialized; };
rt_err_t rt_mutex_init(struct rt_mutex *, const char *, unsigned);
rt_err_t rt_mutex_take(struct rt_mutex *, int);
rt_err_t rt_mutex_release(struct rt_mutex *);
rt_err_t rt_mutex_detach(struct rt_mutex *);
rt_err_t rt_thread_init(struct rt_thread *, const char *, void (*)(void *),
                        void *, void *, size_t, unsigned, unsigned);
rt_err_t rt_thread_startup(struct rt_thread *);
rt_err_t rt_thread_detach(struct rt_thread *);
rt_thread_t rt_thread_self(void);
uint32_t rt_tick_get_millisecond(void);
void rt_thread_mdelay(int);
int rt_kprintf(const char *format, ...);
rt_err_t rt_mq_init(struct rt_messagequeue *, const char *, void *, size_t, size_t, unsigned);
rt_err_t rt_mq_detach(struct rt_messagequeue *);
rt_err_t rt_mq_send(struct rt_messagequeue *, void *, size_t);
rt_err_t rt_mq_recv(struct rt_messagequeue *, void *, size_t, int);
#define MSH_CMD_EXPORT(fn, description) void (*test_export_##fn)(int, char **) = fn
#endif
