# P1真三维物理内核软件验证

已确认计划内的P1交付，开发起点f276000，分支codex/maze-3d-physics。P0正常运行已凭新启动/status/实物照片关闭停点；本批无新硬件动作，不要求重复烧录旧UI。真实三维生产画面和独立任务是P2，不把纯物理内核写成完整游戏。

| 验证 | 结果/记录 |
|---|---|
| 原81项回归 | 原断言保留并通过，见core_first.txt及原驱动host_2026-11-08_204328.txt |
| 新三维27组同源自检 | 全部PASS：XYZ/OBB/旋转、球体面边角扫掠、8m/s薄墙、斜坡、无滑动/摩擦起转、滚阻、能量、复活/通关及完整赛道；同源C在板上启动输出roll3:selftest，尚未在板执行 |
| main实验自检隔离1项 | PASS：三维实验失败不禁用已验证的二维入口 |
| 总核心主机 | 109项PASS，各summary failures=0；Zig0.13、C11、-Wall -Wextra -Werror -O2 |
| SDK v2.5.1 / sf32lb52-lchspi-ulp | 编译PASS，main.bin1156648字节，SHA256 bdb6911b33ece7eb6d840d9c9f937aed1417d164fe7b2f1c7d591e98aee43cec，详见build.txt/firmware.json |
| 课程 | 首轮设计漏追踪编号FAIL保留；补编号后23条严格追踪PASS，最终总检查保留历史8条署名判定/14天间隔及manifest四缺项 |
| P1实物自检/真3D画面/性能/手感 | 未测；与P2接入后集中板测，不从主机PASS推定 |

首轮compile_first.txt编译成功，host_first.txt为25组中的24PASS/1FAIL：两薄盒深重叠可以被最近面修正推出，不一定耗尽预算，原测试夹具假设错误。换成厚墙夹缝不可满足的接触，仍要求有限迭代/速度零且不放宽容差；另补速度/角速度显式预算诊断，compile_second.txt/host_second.txt的26组PASS。补整条演示赛道与main隔离后，core_first.txt为最终109项PASS，原始驱动另保存在evidence/M2/maze_adapter/host_2026-11-08_204328.txt。全部失败与重测保留。

物理是均匀实心球参考：r=0.20m、m=1kg、I=2mr²/5、世界加速度输入、5ms固定步，静态OBB精确最近点及面/圆柱边/球面角扫掠。法线冲量+接触摩擦产生角速度，滚阻耗能，四元数归一化。8m/s、80rad/s速度预算与最多8次冲量/修正有诊断标记；不是无限精度引擎，也未证明目标板实时预算。

复现：`uv run --no-project --with ziglang==0.13.0 --python C:/Users/10436/AppData/Local/Programs/Python/Python312/python.exe python tests/run_maze_host.py --zig`。新增roll3_host直接编译生产physics/scene/selftest；既有runtime/main测试仍是RT/服务替身。SDK沿用外层课程原build_firmware入口与既有ROOT桥接，原配置/构建骨架未改，src Glob自动收集新C；RWX/newlib/entry/DFU警告完整保留。UI/字库未改，未重复旧LVGL输出冒充三维验证。

原始txt工具输出中的表格/空行格式保留，不为通过空白检查修剪日志；源码和文档单独检查，课程提交钩子正常执行。course_checks_after_hw.txt为首笔板证保存后的课程原输出，仍保留历史问题。

物理证据新附件保存在`../p0-delay-2026-11-08/board-run01`，其中原始启动/运行片段用-text保持Git入库字节。该证据只覆盖P0。P1新27项板上自检尚未执行，后续A独立物理任务与B画面接好后统一验证。B任务见`docs/guides/P1真三维物理内核与B渲染接入.md`。负责人已读两份正文并确认hw/feat两次本地保存，实际编号以Git历史为准，本轮不推送。firmware.json与final_audit.txt记录的是提交前核对时的工作树状态。
