/*
 * test_mag.c — 磁场测试：触发 MMC5603 单次测量，将三轴 20 位计数换算为微特斯拉。
 */
#include "lab.h"
#include "lab_units.h"

/* MMC5603：I2C3，7 位地址 0x30。 */
#define MAG_ADDR 0x30
#define MAG_OUT_START 0x00 /* XOUT0..ZOUT2，共 9 字节 */
#define MAG_STATUS1 0x18   /* bit6 Meas_M_Done */
#define MAG_CTRL0 0x1B
#define MAG_CMD_SET 0x08   /* 对磁芯执行 SET 脉冲 */
#define MAG_CMD_TM_M 0x01  /* 触发一次磁场测量 */
#define MAG_PRODUCT_ID 0x39
#define MAG_PRODUCT_ID_VALUE 0x10

/* 核对器件 ID，执行 SET 和单次测量，读取并换算三轴磁场。 */
void lab_mag_run(unsigned action)
{
    (void)action;
    struct rt_i2c_bus_device *bus = lab_i2c("i2c3");
    uint8_t id, data[9];
    rt_err_t err = lab_reg_read(bus, MAG_ADDR, MAG_PRODUCT_ID, &id, 1);
    if (err != RT_EOK || id != MAG_PRODUCT_ID_VALUE) { lab_error(LAB_MAG, "MMC5603 identity", err ? err : -RT_ERROR); return; }
    /* 本项目不启用连续测量；SET 后执行单次磁场测量。 */
    err = lab_reg_write(bus, MAG_ADDR, MAG_CTRL0, MAG_CMD_SET);
    rt_thread_mdelay(2);
    if (err == RT_EOK) err = lab_reg_write(bus, MAG_ADDR, MAG_CTRL0, MAG_CMD_TM_M);
    if (err == RT_EOK && !lab_poll_reg(bus, MAG_ADDR, MAG_STATUS1, 0x40, 200)) err = -RT_ETIMEOUT;
    if (err == RT_EOK) err = lab_reg_read(bus, MAG_ADDR, MAG_OUT_START, data, sizeof(data));
    if (err != RT_EOK) { lab_error(LAB_MAG, "Read magnetometer", err); return; }
    /* 每轴由两个整字节与一个高半字节拼成 20 位计数，再减去中点得到有符号值。 */
    int32_t v[3];
    for (int i = 0; i < 3; ++i)
        v[i] = ((int32_t)data[2*i]<<12 | (int32_t)data[2*i+1]<<4 | data[6+i]>>4) - 524288;
    char converted[3][16], value[80], detail[192];
    for (int i = 0; i < 3; ++i) lab_fixed3(converted[i], sizeof(converted[i]), lab_mag_milliut(v[i]));
    rt_snprintf(value, sizeof(value), "Field (uT)\nX %s\nY %s\nZ %s", converted[0], converted[1], converted[2]);
    rt_snprintf(detail, sizeof(detail), "Raw centered X/Y/Z: %ld / %ld / %ld\n16384 count/G. Factory sensitivity only; offset and hard/soft-iron errors are not calibrated.", (long)v[0], (long)v[1], (long)v[2]);
    lab_publish(LAB_MAG, LAB_PASS, value, detail);
}
