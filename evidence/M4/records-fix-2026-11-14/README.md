# TOP 10漏记修复：REQ-536 / TC-536

负责人报告正常通关无记录并要求直接修复；该反馈更新了上轮整体通过状态中的排行榜子项。此目录全部是软件/主机证据，新板测待负责人提供。

根因链：独立游戏不走ble_power_on，原成绩适配器却只等蓝牙NVDS及SIF_STACK就绪，数据库可以始终没初始化；service又在未ready时直接丢WON，核心还排除曾出现碰撞预算诊断的正常通关。现主动初始化SDK NVDS（含原SDK锁），失败单独重试adaptor；读取以实际DB实例就绪为准。无错误/时间有效的WON进入对应榜单，未存时在RAM保留/显示、恢复与旧榜合并排序、只后台写双槽/校验/失败重试，两关隔离和原数据兼容。

- before-failure.txt：旧实现离线通关丢失的真实断言失败。
- host-init-final.txt及host-raw-final.txt：最终完整逻辑/运行时/适配器回归；实际初始化接口恢复、无SIF_STACK分关读写、离线多局/去重/合并/重载通过。板上纯C自检也增加诊断WON及合并用例。
- menu-final.txt：真实SDK LVGL菜单/榜单/未保存状态/分关/20轮生命周期通过。界面测试使用合成成绩，存储测试使用内存替身，不是真实Flash断电证据。
- sdk-build.txt是补主动NVDS初始化之前的中间构建；sdk-final-env-failure.txt保存普通沙箱工具链启动失败；sdk-final.txt为包含最终初始化修复的官方SDK构建成功原文，不能混用中间镜像。首次自动审批额度错误没有执行命令，随后负责人要求继续、审批恢复后完成最终回归/编译。
- firmware-fingerprints.json及source-fingerprints.json对应最终固件与源码；course-check.txt保存课程检查历史问题。构建标记records-fix-20261114。

没有本版烧录/通关/断电测试。此前未保存且已离开会话的成绩没有历史来源，不能凭空补录；请新固件重新完成一局再验证。RAM待存成绩在真正写后读回成功前仍会因断电丢失，界面不会提前显示SAVED ON THIS DEVICE。
