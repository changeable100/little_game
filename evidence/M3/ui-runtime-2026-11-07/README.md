# B第三阶段P2生产UI软件交付

基线d4520a74a531dccd5afef7eafa486bc0aa40d38f；分支codex/maze-ui-stage3-runtime。用户明确确认A/负责人已核对接口方案。规范和实现前REQ/TC见docs/maze-stage3-p2-interface-proposal.md及docs/01～03；实际调用图：生产菜单→startup gate→UI创建→attach→snapshot/render；按钮→显示run→next_request→action(NULL)→snapshot→render/反馈；成功BACK→release，临时关闭/菜单/外部删除→先detach自己绑定再释放。

## 实际接口和边界

maze_ui_runtime.h公开open/release/last_result/startup_text；maze_ui.h公开视图create/render/destroy、delete handler和feedback；maze_menu_create为生产入口，maze_menu_create_preview仅显式模拟。所有API限定main/LVGL线程，不初始化/取消runtime，不创建游戏时钟/请求号/模型实例或采样服务。boundary_review.txt核对A核心/main/工程配置未改。

视图复制显示快照；适配复制level结构，cells仍为契约规定的长生命周期只读数据。render不递归action/detach。attach拒绝仅清本次UI，成功后读取失败才detach自己绑定。旧页面事件和借用回调context由代际标识拒绝。生产动作全部经runtime分配请求号，不自动重试STALE，耗尽锁定跨重开页面保持。失败提示保留，ERROR原因和动作反馈同时可见。

## 运行命令

Python3.12.14、MinGW GCC4.9.2用于UI；SDK v2.5.1真实LVGL：

```powershell
$env:CC = 'D:\DevC++\Dev-Cpp\MinGW64\bin\gcc.exe'
$env:PATH = 'D:\DevC++\Dev-Cpp\MinGW64\bin;' + $env:PATH
python tests/render.py --sdk D:\sifli\SiFli-SDK\v2.5.1 --out build\maze-p2 --maze-only
python tests/render.py --sdk D:\sifli\SiFli-SDK\v2.5.1 --out build\maze-p2 --runtime-ui
```

纯UI模式仅使用maze_runtime_unavailable.c明确模拟无runtime，不是连接证明。--runtime-ui编译真实maze_ui_runtime.c、maze_runtime.c、controller/model和SDK LVGL；tests/ui_runtime_stubs仅有RT头，绝不混入简化lvgl.h。tests/maze_ui_runtime_test.c替身只实现RT队列/时间/身份与传感器输送；启动枚举、snapshot/请求/动作返回故障使用有标记的链接包装注入，其正常路径调用真实实现。

原69项按A提供的命令：`uv run --no-project --with ziglang==0.13.0 python tests/run_maze_host.py --zig`。本机下载时使用已有127.0.0.1:7890代理和SDK Python3.13的uv临时环境，未改项目依赖/系统代理。此前MinGW4.9运行主入口setjmp测试进程异常（0xC0000028），没有改A测试或降低-O2/断言来通过。

SDK v2.5.1/GCC14.2.1激活后，在project执行`python -m SCons --board=sf32lb52-lchspi-ulp -j8`，环境同evidence/standalone-ui/README.md。build_acceptance.txt为最终构建，main.bin1092656字节，三镜像及源码SHA256见firmware.json。

## 真实结果及失败保留

| 输出 | 结果 |
|---|---|
| pure_initial.txt / pure_final.txt | P1六态异常、20轮导航、2000次render PASS，预览标记保留 |
| connection_initial.txt | 新增测试缺rtthread.h，编译失败，已保留 |
| connection_retry1.txt / connection_final.txt / connection_motion_final.txt | 补齐头文件后连接通过，逐次补矩阵/运动断言 |
| connection_acceptance.txt | 最终生产连接PASS：启动六态/未知值门控、真实START/校准/运动/暂停继续/重开/BACK、时间冻结、PAUSE结算错误重读恢复、旧run不重试、旧页事件、回滚/他人绑定、耗尽、菜单/外部删除、20轮/唯一timer |
| regression69.txt + evidence/M2/maze_adapter/host_2026-11-07_234912.txt | GCC主入口测试进程异常退出；不能当69项通过 |
| regression69_zig.txt | 离线无ziglang缓存，未执行回归 |
| regression69_zig_retry.txt + evidence/M2/maze_adapter/host_2026-11-07_235239.txt | Zig0.13三组原回归69项PASS |
| build_initial.txt | ARM发现NULL未声明，缺stddef.h；失败保留 |
| build_final.txt / build_acceptance.txt | 补头文件后构建PASS，最终布局/提示调整复编PASS；旧SDK RWX/newlib/entry/DFU警告保留 |
| course_checks.txt | 需求追踪PASS；历史8项署名判定与14天断档FAIL；manifest四缺项WARN；原脚本未改 |

输出仅规范行尾空白，诊断/失败不删除。production-*.png和connection-previews.png由真实LVGL渲染生成且已实际查看，输入来自真实runtime接收的合成传感器值。matrix-WON/ERROR-synthetic.png仅验证生产视图控件矩阵，不能当真实runtime通关或输入故障事件。production-timing-error.png则来自真实runtime在主机长dt的PAUSE结算错误。全部为主机测试，不是实物照片。

## 交接与待整机取证

提交正文待负责人读过确认；功能分支发布按当次授权，A审查后合并。本批不新增physical_verification。main原诊断文案仍带旧preview字样（A拥有文件），UI产品路径已改真实数据；建议A集成时同步日志说明，不由B改main。

新固件由负责人实际烧录：完整启动53项逻辑自检、runtime=1/result=0、菜单就绪；首页→ENTER→READY→START→平放CALIBRATING→RUNNING；四向随动、墙阻挡、暂停冻结/继续累计、重开校准、WON冻结、BACK/快速重进。照片/串口/操作顺序及固件上下文放evidence/M3/dynamic-ui/run01，失败另存run02；20轮物理与同状态堆/线程/timer、资源恢复读回仍需A诊断入口。没有精确时刻/轮次就标未知。新增页面适配生命周期由主机连接测试和后续物理流程验证，未自行扩展A拥有的启动逻辑自检或冒称已上板。
