# 第二关r4同源移植与PLAY选关：软件证据

REQ-533/534/535。主机与SDK编译通过；未烧录、未收到当前版本板测。

- sdk-final.txt：官方SDK最终构建成功，main.bin1172332字节，链接器既有RWX/newlib/ftab入口告警保留。
- host-final.txt：完整C逻辑/物理/运行时/记录测试，包含两存档两陡降42.705秒零掉落回放。
- native-menu-final.txt与native-view-final.txt：真实SDK LVGL主机原生回调/渲染与资源测试；LEVEL2-*.png是实际物理回放18姿势，GAME-*.png是UI测试图；榜单数据为合成，不是设备已保存成绩。
- level2-port-report.json：1308顶点/2397面/1603碰撞面/511 BVH节点，最大顶面LOD误差0.009883147780910042m。
- firmware-fingerprints.json及source-fingerprints.json：本轮最终构建镜像和源码指纹。
- course-check.txt：54条追踪通过；既有8条提交格式、14天无提交及manifest四缺项不通过/警告，没有修改历史规避。
- native-view-first-failure.txt：原生测试局部变量重名的初次失败；物理测试失败原文保留在game-menu-records-2026-11-13/host历史日志。极小速度残量和航点驱动问题的实际数值/修复见当日日志，不将测试驱动失败伪装成硬件测试。

烧录和负责人真机操作详见docs/guides/第二关移植与选关-验证-2026-11-13.md。实际显示、手感、帧时和Flash断电保持待真实证据。
