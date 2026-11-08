# P0迟帧容错软件验证（2026-11-08）

负责人已确认独立物理任务方案A、先P0再真3D原型。本批关联REQ-503/REQ-404修订、TC-503，仅改变当前二维游戏的时序策略；仍由主/LVGL线程拥有模型，尚无三维几何或独立物理任务。

## 结果与原始记录

| 项目 | 结果 | 记录 |
|---|---|---|
| 核心主机回归 | 81项PASS：24模型+4地图+31控制器+22 runtime/main | host_third.txt；原始驱动输出`evidence/M2/maze_adapter/host_2026-11-08_192104.txt` |
| 真实SDK LVGL合成视图 | 原中文/非法枚举/六态/20轮2000帧/局部刷新与新暂停原因PASS | ui_view.txt |
| 生产runtime/controller/model+LVGL连接 | 最新采样下200ms手动暂停、300ms自动暂停、新采样后同run恢复，原生命周期/故障断言PASS | ui_production.txt |
| 准备结束重绘预算 | 请求84000、最大flush39000、总106197像素，原判据保持PASS | ui_view.txt |
| 中文字库 | 273字形×16/20/28字号，生成与可复现检查PASS | font_generation.txt、font_check.txt |
| SDK v2.5.1，sf32lb52-lchspi-ulp | 编译成功；main.bin1142680字节 | build.txt、firmware.json |
| 课程检查 | 21条严格需求追踪PASS；全量仍报历史8条提交规范、14天提交间隔及manifest四缺项 | trace.txt、course_checks.txt |
| 新固件板端/真实手感 | 未验证，不登记物理PASS | 按操作文档另存board-run01，不用主机图替代 |

真实SDK LVGL测试的RT/传感器为替身，部分API错误为明确的链接注入，不能证明硬件调度、屏幕传输时间、物理堆或3D物理正确。`host-delay-paused.png`是生产连接测试的合成截图，已人工查看；`host-synthetic-delay-paused.png`为纯视图合成，两者都不是板子照片。

## 失败与重测保留

1. `host_before.txt`：新增TC-503在旧代码上复现短迟帧拒绝/严重中断终止两项FAIL；原始`host_2026-11-08_191420.txt`保留。
2. `host_first.txt`：新控制器用例通过，旧runtime仍期待101ms结束而FAIL；按负责人确认的新需求更新该断言，原始`host_2026-11-08_191723.txt`保留。
3. `host_second.txt`：最新发布样本的PAUSE测试失败，核对后为替身未释放旧worker，重启没有进入校准；只修正替身停止完成前提，原始`host_2026-11-08_191929.txt`保留。
4. `host_third.txt`：最终81项PASS，各套summary failures=0。

已有中文全屏准备切换的真实104～108ms失败仍在M3/board-run01，后来的旧二维中文成功照片在M3/board-run02；本次修改需求不把先前失败改写通过。迟帧容错仍要求有效新鲜输入，保留100ms采样年龄、32稳定样本/5秒校准，基础模型单批100ms保护也不变。

## 复现与工具边界

在本工作副本运行：

```powershell
uv run --no-project --with ziglang==0.13.0 --python C:\Users\10436\AppData\Local\Programs\Python\Python312\python.exe python tests/run_maze_host.py --zig
uv run --no-project --with fonttools==4.68.0 --with pillow==12.3.0 --python C:\Users\10436\AppData\Local\Programs\Python\Python312\python.exe python tools/generate_maze_font.py --sdk D:\SiFli-SDK\v2.5.1 --check
```

UI的通用驱动是未改的`tests/render.py --sdk D:/SiFli-SDK/v2.5.1 --maze-only`和`--runtime-ui`。本机Windows没有可用的GNU --wrap，使用外层忽略build目录中的既有`check_p2_timing_ui.py`与`check_p2_integration_ui.py`启动器：补Zig的cc子命令；生产连接在PE链接时只重命名四个runtime定义并提供转发到原__wrap_*的别名，原测试体/断言/生产函数体保持。启动器、驱动及本次用例源码指纹见verification_tools.json。

固件使用外层原课程`build_firmware.py`，既有`build_maze_a2.py`只将ROOT指向当前工作副本；先加载SDK export，再以Python3.12执行。未改project骨架、Kconfig、proj.conf、课程配置/检查脚本。RWX、newlib未实现调用、entry及缺DFU警告按原文保留，没有屏蔽或删除功能。

原课程日志/检查器通过外层既有`check_maze_a2.py`路径桥接执行，原文件和门限未修改。缺失的学号/组号/提交时间/会话导出路径不猜填，历史合法工具邮箱被占位符规则命中及14天间隔不靠改历史绕过。

原始build.txt表格及course_checks.txt/course_checks_after_hw.txt保留工具生成的行尾空格，不为格式检查修剪原文；源码/文档空白单独检查通过，课程提交钩子照常执行。

负责人已读并确认hw/feat两份正文，按两个独立提交保存，实际编号以Git历史为准；本轮不推送。firmware.json的development_base只是开发起点49dda19，实际二进制对应记录的工作树源码哈希。烧录与B契约v2交接见`docs/guides/P0迟帧容错-烧录验收与B接口.md`。
