/*
 * test_uart.c — 串口测试：验证 UART2 发送、接收或回环，结束后恢复与控制台共用的引脚。
 */
#include "lab.h"
#include "bf0_hal.h"
#include "drv_io.h"
#include <string.h>

typedef struct {
    lab_status_t state;
    const char *value;
    char detail[192];
    char rx[48];
    size_t got;
} uart_result_t;

/* TX：每秒发一帧，共三帧；写入成功只说明进入驱动，需对端终端人工确认。 */
static void uart_tx_test(rt_device_t uart, uart_result_t *r)
{
    const char frame[] = "LAB-TX\r\n";
    unsigned sent = 0;
    for (int i = 0; i < 3 && !lab_cancelled(); ++i) {
        if (rt_device_write(uart, 0, frame, sizeof(frame)-1) != sizeof(frame)-1) break;
        sent++;
        for (int j = 0; j < 50 && !lab_cancelled(); ++j) rt_thread_mdelay(20);
    }
    r->state = sent == 3 ? LAB_OBSERVE : LAB_FAIL;
    r->value = sent == 3 ? "3 frames sent" : "TX incomplete";
    rt_snprintf(r->detail, sizeof(r->detail), "Sent LAB-TX + CRLF %u times at 115200. Confirm all three frames on the receiving terminal.", sent);
}

/* RX / 回环：回环先自行发送再比对；RX 等待外部发来 LAB-RX + LF。 */
static void uart_rx_test(rt_device_t uart, bool loopback, uart_result_t *r)
{
    char tx[48];
    if (loopback) rt_snprintf(tx, sizeof(tx), "LAB-%08x-loopback\n", (unsigned)rt_tick_get());
    else rt_snprintf(tx, sizeof(tx), "LAB-RX\n");
    size_t want = strlen(tx);
    bool sent = !loopback || rt_device_write(uart, 0, tx, want) == want;
    lab_publish(LAB_UART, LAB_RUNNING, loopback ? "Waiting for echo" : "Send LAB-RX + LF",
                loopback ? "PA19 TX must be connected to PA18 RX. Timeout: 1.5 s." : "115200 8N1. Send the exact 7-byte frame within 8 seconds.");
    rt_tick_t start = rt_tick_get();
    unsigned timeout = loopback ? 1500 : 8000;
    while (sent && r->got < want && !lab_cancelled() &&
           (rt_tick_t)(rt_tick_get()-start) < rt_tick_from_millisecond(timeout)) {
        /* read 返回本次实际字节数；只读剩余空间，允许一帧分多次到达。 */
        r->got += rt_device_read(uart, 0, r->rx + r->got, want - r->got);
        rt_thread_mdelay(10);
    }
    bool match = sent && r->got == want && memcmp(tx, r->rx, want) == 0;
    r->state = match ? LAB_PASS : LAB_FAIL;
    r->value = match ? (loopback ? "Loopback verified" : "RX verified") : "Frame mismatch / timeout";
    rt_snprintf(r->detail, sizeof(r->detail), "Expected %u / received %u bytes. %s Console pinmux restored after test.",
                (unsigned)want, (unsigned)r->got, loopback ? "Exact loopback comparison." : "RX expects LAB-RX followed by LF only.");
}

/* 配置并打开 UART2，执行限时收发，关闭设备并恢复控制台引脚。 */
void lab_uart_run(unsigned action)
{
    rt_device_t uart = rt_device_find("uart2");
    if (!uart) { lab_error(LAB_UART, "Find uart2", -RT_ENOSYS); return; }
    struct serial_configure cfg = RT_SERIAL_CONFIG_DEFAULT;
    cfg.baud_rate = 115200;
    /* 把波特率等配置下发到串口驱动，再打开读写与中断接收。 */
    rt_err_t err = rt_device_control(uart, RT_DEVICE_CTRL_CONFIG, &cfg);
    if (err != RT_EOK) { lab_error(LAB_UART, "Configure UART", err); return; }
    err = rt_device_open(uart, RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_INT_RX);
    if (err != RT_EOK) { lab_error(LAB_UART, "Open UART", err); return; }
    /* PA18/19 与 UART1 控制台共用：只在测试窗口临时切换。 */
    HAL_PIN_Set(PAD_PA18, USART2_RXD, PIN_PULLUP, 1);
    HAL_PIN_Set(PAD_PA19, USART2_TXD, PIN_PULLUP, 1);
    char discard[32];
    uart_result_t r = {.state = LAB_FAIL, .value = "UART test failed"};
    /* 有限次清理旧接收数据，避免上一轮残留被当成本轮应答。 */
    for (int i = 0; i < 8; ++i)
        if (!rt_device_read(uart, 0, discard, sizeof(discard))) break;
    if (action == 0) uart_tx_test(uart, &r);
    else uart_rx_test(uart, action == 2, &r);
    /* 等待最后一帧离开发送器，再恢复 BSP 的 UART1 引脚。 */
    rt_thread_mdelay(20);
    rt_err_t close_err = rt_device_close(uart);
    HAL_PIN_Set(PAD_PA18, USART1_RXD, PIN_PULLUP, 1);
    HAL_PIN_Set(PAD_PA19, USART1_TXD, PIN_PULLUP, 1);
    if (close_err != RT_EOK) { lab_error(LAB_UART, "Close UART", close_err); return; }
    if (r.got) {
        rt_kprintf("[lab:UART] RX hex:");
        for (size_t i = 0; i < r.got; ++i) rt_kprintf(" %02X", (unsigned)(uint8_t)r.rx[i]);
        rt_kprintf("\n");
    }
    if (lab_cancelled()) lab_publish(LAB_UART, LAB_IDLE, "UART test stopped", "Device closed; UART1 console pinmux restored.");
    else lab_publish(LAB_UART, r.state, r.value, r.detail);
}
