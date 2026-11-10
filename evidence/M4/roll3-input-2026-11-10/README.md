# TC-511 三维校准后输入异常
学生报告可以进入并短暂显示游玩中，随后输入异常。见board-feedback.md。未收到上版串口，实际触发原因仍未知。

代码确认主LVGL优先级19、物理线程18、lab_io 22，渲染连续执行时有饿死采样的风险。新固件仅在3D采样期间将lab_io提高到17，退出恢复22。100ms样本新鲜度条件不变。三维校准成功已经要求获得自己的IMU会话，二维未进入时没有采样；本轮无需以删除二维释放内存。主机121项回归通过，新增故障分类、100ms边界和会话丢失验证。

## 板型配置
sf32lb52-lchspi-ulp / SF32LB525UC6。立创规格标称主核240MHz、小核24MHz、576KB片上SRAM，板上16MB NOR Flash、8MB OPI PSRAM、1.85英寸390×450 AMOLED与LSM6DS3TR-C六轴IMU。当前SDK链接LVGL RT-Thread分配实现，未在map见专用LVGL PSRAM池，不按8MB估算可分配连续块。
- [SiFli板级文档](https://docs.sifli.com/projects/sdk/latest/en/sf32lb52x/supported_boards/boards/sf32lb52-lchspi-ulp/doc/index.html)
- [立创硬件规格](https://lckfb.com/activities/lckfb-hspi-sf32lb52-ulp/you-hui)
3--------------------------------------------------------
## 真机复测
沿用[COM与SDK准备](../p3-tuning-2026-11-09/README.md)，核验文件改为本目录 **firmware.json**。烧录并用1000000 baud、8N1开始记录串口，再复位。点3D挑战，平放完成校准，静置数秒再轻倾，记录至少10秒。成功时应看到 `[maze:imu] ... boost_priority=17 result=0`、`[roll3] ... state=2`，游玩连续。若仍输入异常，保留从boost_priority到 `[roll3:input]`、`[maze:imu] ... stopped` 的日志。原因字段分别为sample_timeout、invalid_sample、sequence、owner_lost、cal_timeout或physics。accepted_seq=0表示尚无已接受样本。材料放本目录board-run01。
