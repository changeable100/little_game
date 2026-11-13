/* Runs only in the existing lab_io slot. Register plan matches test_game.c
 * and SDK v2.5.1 customer/peripherals/sensor/LSM6DSL/libraries/lsm6dsl_reg.h.
 * Physical part identity/orientation still require board evidence. */
#include "lab.h"
#include "lab_game.h"

static int16_t word(const uint8_t *p)
{
    return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

void lab_maze_motion_run(unsigned session)
{
    struct rt_i2c_bus_device *bus = lab_i2c("i2c3");
    uint8_t id = 0, old[3], data[12];
    lab_motion_t sample = {0};
    rt_err_t err;
    unsigned i, missed = 0;
    bool saved = false;
    sample.session = session;
    err = lab_reg_read(bus, 0x6A, 0x0F, &id, 1);
    rt_kprintf("[maze:imu] run=%u WHO_AM_I=0x%02x result=%d\n", session, id, err);
    if (err == RT_EOK && id != 0x6A) err = -RT_ERROR;
    if (err != RT_EOK) goto done;
    for (i = 0; i < 3 && err == RT_EOK; ++i)
        err = lab_reg_read(bus, 0x6A, 0x10 + i, old + i, 1);
    if (err != RT_EOK) goto done;
    saved = true;
    err = lab_reg_write(bus, 0x6A, 0x12, (old[2] & ~0x02) | 0x44);
    if (!err) err = lab_reg_write(bus, 0x6A, 0x10, 0x40);
    if (!err) err = lab_reg_write(bus, 0x6A, 0x11, 0x40);
    while (!err && !lab_cancelled())
    {
        uint8_t ready = 0;
        err = lab_reg_read(bus, 0x6A, 0x1E, &ready, 1);
        if (!err && (ready & 3) == 3)
        {
            err = lab_reg_read(bus, 0x6A, 0x22, data, sizeof(data));
            if (!err)
            {
                for (i = 0; i < 3; ++i)
                {
                    sample.g[i] = word(data + i*2) * 0.00875f;
                    sample.a[i] = word(data + 6 + i*2) * 0.000061f;
                }
                /* Do not wrap sequence within one acquisition session. */
                if (sample.sequence == UINT32_MAX) { err = -RT_ERROR; break; }
                ++sample.sequence;
                sample.valid = true;
                lab_motion_publish(&sample);
                missed = 0;
            }
        }
        else if (!err && ++missed > 5) err = -RT_ETIMEOUT;
        rt_thread_mdelay(20);
    }
done:
    sample.valid = false;
    lab_motion_publish(&sample);
    if (saved)
        for (i = 0; i < 3; ++i)
        {
            rt_err_t restore = lab_reg_write(bus, 0x6A, 0x10 + i, old[i]);
            if (restore != RT_EOK)
            {
                rt_kprintf("[maze:imu] restore reg=0x%02x result=%d\n", 0x10+i, restore);
                if (!err) err = restore;
            }
        }
    rt_kprintf("[maze:imu] run=%u stopped result=%d settings_saved=%d\n", session, err, saved);
    if (err) lab_error(LAB_IMU, "Maze IMU / restore", err);
    else lab_publish(LAB_IMU, LAB_OBSERVE, "Maze sampling stopped", "Settings restored; physical response awaits confirmation.");
}
