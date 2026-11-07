# B第三阶段首批P0/P1交付

日期2026-11-07；基线cfa87d01a4f5c7e560ed3314469a06c5847ef1fb，分支codex/maze-ui-stage3-render。用户明确批准先交P0/P1。接口/REQ/TC详见docs/maze-stage3-render-review.md。代码仅改maze_ui.c/.h及maze_ui_test.c，A代码和构建配置不变（boundary_review.txt）。

## 命令与依赖

仓库根目录，Python3.12.14、MinGW GCC4.9.2及SDK真实LVGL v9：

```powershell
$env:CC = 'D:\DevC++\Dev-Cpp\MinGW64\bin\gcc.exe'
$env:PATH = 'D:\DevC++\Dev-Cpp\MinGW64\bin;' + $env:PATH
python tests/render.py --sdk D:\sifli\SiFli-SDK\v2.5.1 --out build\maze-p1 --maze-only
python tests/run_maze_host.py --cc $env:CC
```

SDK v2.5.1 / GCC14.2.1的已激活环境，在project目录执行 `python -m SCons --board=sf32lb52-lchspi-ulp -j8`；具体本机环境初始化沿用evidence/standalone-ui/README.md，不改proj.conf。main.bin1089824字节；三镜像与源文件SHA256见firmware.json。构建完整输出仅规范行尾空白并注明。

## 结果与预览输入

- ui_host_initial.txt：首次六态/异常/生命周期通过。ui_host_final.txt：将2000次重复渲染补强为六态轮换后的最终通过输出。
- tests/maze_ui_test.c保存可执行输入：run0/revision1/level1/r0.20；READY和CALIBRATING在起点，elapsed20ms显示0s；RUNNING/PAUSED/ERROR圆心(4.5,1.5)、elapsed12345ms；WON圆心(14.5,13.5)、elapsed12345ms。输入错误、关卡错误（NULL关卡）、计时错误、无错误详情分别验证。最大时间UINT32_MAX显示4294967s；未知枚举/非有限坐标/非法格值/越界/ID错误安全提示且球隐藏。
- state-READY/CALIBRATING/RUNNING/PAUSED/WON/ERROR.png和six-states.png：从生产LVGL渲染PPM转换，最终截图已实际查看；全部明确为合成数据。P1游戏动作仍禁用，PAUSED显示RESUME，BACK仅关闭模拟页。
- 保留20轮独立菜单/页面重入及2000次六态轮换，重复revision不增加对象/timer，新实例初始revision可显示，临时快照修改和等待不让UI自行计时。
- logic_regression.txt：原有58项生产逻辑/runtime边界回归通过，runner原始新日志保留在evidence/M2/maze_adapter/host_2026-11-07_211724.txt；RT/LVGL传输替身不证明硬件行为。
- build.txt：SDK成功，RWX/newlib syscall/entry/DFU旧警告完整保留。物理验证未执行，指纹不等于运行通过。

## 课程检查与局限

本B副本无课程scripts/config。本次用忽略build中的参数化路径桥接器（显式--course、--repo），只重定向ROOT/配置读取及原汇总脚本的子脚本路径，执行D:/course_starter原脚本；未改原脚本、钩子或配置。包装器不作为可移植交付工具。

course_checks_initial_bridge_failure.txt保留首次桥接器main签名错误和解释器缺PyYAML结果；course_checks_final.txt为修正本地桥接器并用SDK现有PyYAML解释器后的实际检查：需求追踪PASS，8项历史署名判定、历史14天断档FAIL，manifest四个缺项WARN；没有报告总检查通过。devlog_refresh.txt记录原new_devlog.py对共享当天日志的刷新；B工作以独立追加小节记录，保留A日志原文。

本批B接口核对已完成，A签署待审；没有生产attach、按钮请求、结果重读或detach适配，也没有更改main启动顺序。当前固件首页仍进入READY模拟预览，六态截图由主机driver注入。下一批先联评P2/P3再安排整机烧录，不把主机WON/ERROR图当真机事件；M3仍未完成。

暂存差异检查发现课程检查输出空行的尾随空白，已仅规范行尾，诊断内容与FAIL/WARN结论未改。
