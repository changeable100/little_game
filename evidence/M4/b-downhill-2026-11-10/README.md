# M1 三维下坡关卡：软件验收材料

2026-11-10，REQ-513 / TC-513，基线 d422c3c，分支 codex/roll3-downhill-b。关卡坐标及每张截图姿势见 [B 交接](../../../docs/guides/B三维下坡M1交接.md)。图像为真实 SDK LVGL 主机出图，状态是合成快照。

| 证据 | 结果 |
|---|---|
| core-initial.txt / core-final.txt | Zig 0.13.0；初次路线 PASS，收紧顶角/接缝误差计算后全量重测，129 项 PASS。对应命令与退出码同时保存在输出指向的 evidence/M2/maze_adapter 原日志。 |
| route-trace.txt | 同源测试的仅打印诊断副本；六条接缝端点偏差、六次越缝、两次低速转向及 32280ms WON、checkpoint=1、falls=0。 |
| view-initial.txt、M1-*.png | 实际 390×450 出图并逐图查看；原 LVGL 七态、20 轮、控件、分配失败和运行/静态渲染策略通过。 |
| edge-initial.txt | 旧 MinGW 编译器没有 C11 的 timespec_get / TIME_UTC，未执行对比；失败保留。 |
| edge-zig.txt | 原判据不变。36 姿势，局部边缘 348974/348975 与参考在量化容差内，密集对比 48000/48000；100/50 行逐像素一致，游玩分带与整深度单采样一致。 |
| sdk-build.txt / firmware.json | 原课程构建脚本运行 SiFli SDK v2.5.1，板型 sf32lb52-lchspi-ulp，编译通过。main.bin 1229288 字节；旧 RWX、newlib、entry 告警保留。三镜像指纹与源码指纹可核对，镜像不入库。 |
| source-boundary.txt / course-checks.txt / trace.txt | A 负责的物理、相机、采样、任务和 SDK 骨架无改动，物理验证原记录一致。课程总检查与严格追踪另存。 |

复现（SDK 环境按项目已安装位置设置）：

```powershell
uv run --no-project --with ziglang==0.13.0 python tests/run_maze_host.py --zig
python tests/render.py --sdk D:/sifli/SiFli-SDK/v2.5.1 --roll3-view --out build/roll3-downhill-view
uv run --no-project --with ziglang==0.13.0 python -m ziglang cc -std=c11 -O2 -Isrc tests/roll3_edge_compare.c src/roll3_scene.c src/roll3_physics.c -o build/roll3-m1-edge.exe
./build/roll3-m1-edge.exe
```

要复现附加打印，可从仓库根目录执行本目录的 route-trace.py，再用 C11 编译器将生成的 build/m1_trace.c 与 src/roll3_scene.c、src/roll3_physics.c 一起编译（-O2 -Isrc -lm）。标准回归不依赖该副本。主机渲染毫秒不等于板上帧率；真实操控和帧时待 A 合入后测试。

构建命令为课程 `build_firmware.py`（在集成工作副本通过本机忽略目录的 course_bridge.py 指定仓库根目录），默认调用 `scons --board=sf32lb52-lchspi-ulp -j8`。生成日志仅整理行末空格；失败、告警、结果与命令保留。
