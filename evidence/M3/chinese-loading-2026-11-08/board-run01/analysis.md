# 中文准备界面首次实测：时序失败（2026-11-08收到）

用户直接粘贴20行，保存为user_log_original.txt；这是会话内联文本，不是完整串口附件，换行由归档形成。另反馈“现在玩不了了，一放平就加载错误”。按e0a6247烧录回复上下文关联，缺新启动日志、照片、独立固件指纹与精确操作时间；不补猜测。

四次均WHO_AM_I=0x6a/result0，随后首次TIMING记录：run9 gap104/handler102；run10 gap108/handler106；run11 gap107/handler105；run12 gap105/handler103；均render1、prep0、wait2、valid1/rv1。实际故障间隔104～108ms超过原100ms上限，不是命令输入速度问题。记录由从RUNNING进入TIMING ERROR路径触发，故至少已结束CALIBRATING；没有cal字段，不额外虚构本次稳定样本计数或精确失败时刻。服务随后停止并报告恢复配置调用result0/settings_saved1，不等于设备掉线/损坏或已独立读回配置。

renderer只占约1ms，完整handler102～106ms；handler包含LVGL布局、绘制、flush等待及调度等，不能仅用差值断言唯一硬件瓶颈。源码确认全屏390×450 prepare_panel从CALIBRATING隐藏时会使整屏失效，现有主机FULL显示模式也不能体现局部刷新代价；之前只补静止/球位的部分刷新断言，漏了准备切换。本次生产可玩性FAIL，旧英文run03与主机PASS不覆盖。

局部修复方向：同页准备卡片缩小显示区域，保留真实状态/中文/失败重载与清理，新增PARTIAL模式下CALIBRATING→RUNNING的重绘请求/flush区域上限测试，避免全屏揭示；不改100ms、校准或时间anchor。SDK源确认旧API驱动可DIRECT/PARTIAL，另有renderlist FULL实现，当前具体宏与真实显示耗时需谨慎关联；不改SDK/配置。修复后新镜像仍须board-run02实测，不能从主机面积比例推造真实耗时。

后续构建核对：当前生成rtconfig未定义DRV_EPIC_NEW_API，定义LV_FB_TWO_NOT_SCREEN_SIZE与LV_FB_LINE_NUM100，按已读SDK SConscript/旧lv_lcd.c走PARTIAL。局部修复再以主机同等100行像素容量复核，通过切换面积预算；仍不能把主机flush矩形视作板端传输毫秒或重测已通过。
