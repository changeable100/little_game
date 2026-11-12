# M3 坡边连续性与游玩耗时修复候选（REQ-521 / TC-521）

负责人确认问题来自 m3-slim 细长槽版：整体卡顿，近垂直冲向坡边会突然减速。本次没有新视频或串口，不记录实机通过。

完整源码、工程和测试：`D:/course_starter/build/m3-fluid-20261111`。旧 m3-slim 源码、固件和各试玩目录保持。Git 工作区仍为 `build/maze-integration`，其 src 尚未集成最新完整关卡，不能直接从那里编译新版。修复补丁以完整 m3-slim 为基线，不覆盖已有工作区改动。

## 碰撞定位和结果

原版 5m/s 连接坡入弧槽回放，在第 102 个 5ms 步发生 4.64488→0m/s。轨迹中没有撞到导栏，连续槽面产生多次微小法线冲量，8 次碰撞预算用完后触发 limited 清零。原始轨迹和新增两项同源自检的 FAIL 保留在 `edge-probe-baseline.txt`、`solver-trace-baseline.txt`、`core-before.txt`。

修复保持 8 次安全预算。低法向速度的持续曲面接触改为支撑运动：每段法线变化受限，步末校正不超过 1mm；必要时缩短该段时间，步末仍投影到原 SKIN 并施加接触冲量。真实盒体/挡边、较大法向撞击和尖锐边缘继续使用扫掠碰撞。首版修复仍有两处斜向耗尽，追加有界支撑小步后解决；相关失败和重测分别保留。

`physics-compare.txt` 单独链接未改动的旧物理实现，从完全相同的清零前快照各执行一步：旧版 4.644880→0（limited=1），新版 4.644880→4.643016（limited=0）。同一快照重复 20000 次，主机平均单步 7.190→0.528μs，约降低92.7%；不代表整关物理平均或板端耗时。

- `core-final.txt`：151 条 TC PASS（原 147 条 + 4 条高速/斜向同源启动自检），全部核心、M1/M2/M3 通关、回落、挡边、复活和输入/迟帧保护通过。板上新自检尚未执行。
- `edge-probe-assert-final.txt`：800 组直坡四缘、0°/±5°/±15°、0.5/2/5/7.5m/s，400 组隔离开放边缘 + 400 组整关轨迹。限额耗尽 0/800，隔离开放边缘突然损失超过 25% 为 0/400。整关的 16 组速度下降包含真实导栏及落到其他路面，未把这些轨迹记作开放边缘通过，也未代替真机手感判定。
- 新增诊断最初遗漏 Zig 优化下的 NDEBUG 处理；检查到后显式取消 NDEBUG，重新编译并执行上述断言，最终输出另存，不用未启用 assert 的日志代替。

## 绘制与未完成目标

裁去当前分带之外的物体/三角形和离屏路面顶面；复用相邻棋盘格、弧槽和球的顶点；减少扫描行外部像素工作，保留边界的原浮点加法顺序；内部使用倒数深度增量；RGB565 平滑合并红蓝运算并跳过恒色边距。画面仍为 240×200、1.5 倍输出，持久画面缓冲仍为 153600/129600 字节，两档不变。请求周期 50→33ms，实际 FPS 未知。

`edge-final.txt`：原边缘质量、M1 慢移抖动降低 68.9%、100/50 行逐像素一致、哨兵及 M2/M3 36 帧通过。`perf-final.txt`：旧/新 42 帧共 2,016,000 像素完全相同，六处主机平均约 0.4937→0.4137ms，降低约 16.2%。**原定至少降低 25% 的性能断言仍 FAIL，未修改阈值，不记 TC-521 整项通过。** 早期多轮未达目标保留。一次直接跳算扫描行起点产生 10 个像素差异，方案已撤回；最终保留原边界累加顺序。主机耗时不能外推为黄山派帧时。

每秒 `[roll3:frame]` 在原样本年龄、绘制和帧间隔之外，增加 `cycle_max_ms`、`catchup_steps_max`、`limited_steps`。入口打印 `build=m3-fluid-20261111 refresh_ms=33`，便于核对版本。没有每个物理步打印，避免诊断本身持续拖慢游戏。

真实 SDK LVGL 七态、六处 M3 主机图和 20 轮页面资源释放通过（`view-final.txt`）。`M3-fluid-host.png` 为主机模拟图，已实际查看，不是实拍。SDK 编译成功，main.bin 1,254,416 字节，SHA256 `c7a819f4d48820fb45405bbf15598cfff660ad0ff864cbed489df070352358f8`；三镜像、源码及补丁指纹见 `firmware.json`。原 SDK 既有警告保留，无新增 roll3 警告。

补丁 `m3-fluid.patch` 在旧 m3-slim 上正向检查及新 m3-fluid 上反向检查通过，未实际应用到旧快照。旧 main.bin 哈希与交接仍一致，工程骨架/配置及 src/SConscript 与旧版逐文件一致。完整关卡集成到 Git 和提交尚未执行；本目录有实际提交正文草稿，须负责人阅读后确认，未沿用计划批准作为提交正文审阅。

课程原 `new_devlog.py --update` 已刷新 2026-11-11 日志。`check_all.py` 通过原路径包装器执行：需求追踪 PASS；仍为原有8条历史提交格式、14天提交间隔 FAIL 和 manifest 四缺项 WARN，详见 `course-checks.txt`。本次新增 REQ-521/TC-521 可追踪，保护文件无改动，未改检查器消除既有问题。

## 烧录与最少复测

先关闭串口助手，USB 连接 USB-to-UART 口，提示时填 COM 后的数字。

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy RemoteSigned -Force
$env:Path = "$env:USERPROFILE\.local\bin;$env:Path"
. 'D:\SiFli-SDK\v2.5.1\export.ps1'
Set-Location 'D:\course_starter\build\m3-fluid-20261111'
& '.\project\build_sf32lb52-lchspi-ulp_hcpu\uart_download.bat'
```

串口波特率 1000000。回传一次启动至游玩的原始日志，保留 `[roll3:open]`、`[roll3:frame]` 和新 TC-521 的 PASS/FAIL。录一段包含原来会顿挫的位置、近垂直冲边、连接坡进弧槽以及普通直坡慢滚的视频，说明问题是否仍出现；不要求为凑完整通关重复多轮。证据存入本目录 `board-run01/`，尚未收到时不创建假的实测结果。

这一步需要你在真实硬件上验证并回传证据（日志/照片/截图，存进 evidence/），我需要看到证据才能继续。

## 重现

核心：在新版目录执行既有 `tests/run_maze_host.py --zig`，使用安装了 ziglang 的 Python。实际环境为 `C:/Users/10436/AppData/Local/uv/cache/archive-v0/BwMOkx4NJxyDmgJP/Scripts/python.exe`。

SDK LVGL：用同一 Python 执行 `tests/run_roll3_sdk_view.py`。两个主机独立程序按原日志中的完整编译命令执行；`roll3_edge_compare.c` 是原有画面回归，`roll3_edge_probe.c` 是新增冲边扩展，`roll3_perf_compare.c` 对比原版/新版渲染器。性能对照需要先把旧渲染器中的 `roll3_renderer_` 公共符号前缀改为 `baseline_renderer_` 后单独链接（`diagnostics/renderer_baseline.c` 保留实际使用副本）。

SDK：激活 export.ps1 后通过原课程 `scripts/build_firmware.py` 指定 ROOT 为新版目录；没有修改 proj.conf、SConstruct、Kconfig、rtconfig.py 或课程保护文件。

```powershell
python -c "from pathlib import Path; import sys; sys.path.insert(0,'D:/course_starter/scripts'); import build_firmware; build_firmware.ROOT=Path.cwd(); raise SystemExit(build_firmware.main([]))"
```
