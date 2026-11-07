# 最新：2026-11-07完整四向符号修复

学生确认本次仍按右/左/上/下且朝向不变，完整记录证明旧正号推断反向。现屏幕映射为x=-ay、y=-ax，原生ax/ay零偏和滤波不变。下方11月6日正号说明是历史状态，以此最新段为准。

重新烧录同目录uart_download.bat三镜像，先平放restart，等校准完成，再只做一次“右边缘略放低约2秒→回平”，保存新日志。无需实时盯日志；由agent检查tilt_x是否为正。确认该方向后再继续其他方向和第五步。最新镜像指纹：evidence/merge-2026-11-06/board-run04/sign_fix_software/firmware.json。

新启动自检为模型24项、地图4项、适配器25项；主机回归58项。单图静态预览仍不显示实际模型随动。新符号尚待板端复核，不能用旧日志冒充PASS。

---

# 2026-11-06轴向与日志修复版

学生确认实际倾斜顺序为右、左、上、下；结合原始读数，屏幕映射改为x=ay/y=ax，均不反号。偏置仍按传感器ax/ay记录，tilt_milli按屏幕x/y输出。诊断拆为[maze:diag]状态/时间、[maze:imu]原始加速度/年龄、[maze:gyro]角速度、[maze:input]偏置/归一化/球位置四条短行，用run/seq关联，不改变500ms报告周期。

请重新烧录本目录最新uart_download.bat生成的完整镜像，平放后maze_diag start，按右/左/上/下倾斜并回平。右应tilt_x正、左负、上tilt_y负、下正；保存包括四种短行的原文，再做暂停/继续/重开/返回。原合并版固件指纹见旧firmware.json，只描述修复前版本；修复版新指纹保存在board-run01/fix_software/firmware.json。新固件未经上板，不把旧读数当成修复后PASS。

---

# A 第二阶段：IMU 联调与 B 接入

2026-10-31。负责人王若豪，分支`codex/maze-a-stage2`，基于共同分支`feat/maze-integration`的943e762。
本轮实现输入校准、动作适配、生产时钟和采样服务。主机56项通过，SDK编译通过；新固件尚未上板，不能宣布真实IMU、轴向或UI通过。

## 1. 本机操作目录

本轮实际工程在`D:\course_starter\build\maze-integration`。外层`D:\course_starter`仍是原A工程和未提交报告材料。**不要在外层执行旧镜像的下载脚本。**

打开PowerShell：

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy RemoteSigned -Force
$env:Path = "$env:USERPROFILE\.local\bin;$env:Path"
. D:\SiFli-SDK\v2.5.1\export.ps1
cd D:\course_starter\build\maze-integration\project
scons --board=sf32lb52-lchspi-ulp -j8
```

本次agent实际复用外层原课程`build_firmware.py`，通过`D:\course_starter\build\build_maze_a2.py`仅指定工作副本ROOT，命令与板型一致，没有改构建骨架或配置。新clone也可在其`project`目录执行上述scons。

## 2. 烧录与启动（学生操作）

1. 板子连接USB-to-UART接口。退出旧游戏、停止Lab连续测试，断开VS Code串口监视器，让出端口。
2. 在已export的同一PowerShell执行：

```powershell
cd D:\course_starter\build\maze-integration
.\project\build_sf32lb52-lchspi-ulp_hcpu\uart_download.bat
```

3. 提示串口号时填写实际数字，如COM4填`4`；以本机当前设备为准。脚本下载bootloader、main和ftab三份，地址由工程生成，不手改。若工具明确提示复位/下载模式，按提示操作；不能把串口监视器的“复位”按钮当作板上复位保证。
4. 下载成功后重新连接串口，波特率`1000000`，发送行尾CRLF。在板上复位或shell输入`reboot`，保存从启动到汇总的完整日志。
5. 预期三组汇总：`[maze:selftest]`原24项、`[maze:level:selftest]`4项、`[maze:adapter:selftest]`新24项，均为`summary PASS failures=0`。这些只验证隔离逻辑，不能代替真实传感器操作。任一失败时运行时禁用，保留失败原文。

当前屏幕仍是Lab，不会自动出现迷宫；B的maze_ui尚未实现。下面的诊断可以先验证真实输入链路。

## 3. 真实 IMU / 计时取证

在板子平放、Lab工作槽空闲时，串口逐条发送（每条回车）：

```text
maze_diag start
maze_diag status
```

`queued result=0`只表示入队成功；必须再看`[maze:command] action=... result=...`和`[maze:diag]`状态。请求编号由UI线程分配，shell不直接推进模型。

| state数字 | 含义 | 验证重点 |
|---|---|---|
| 0 | READY | 尚未开始，elapsed为0 |
| 1 | CALIBRATING | 放平不动；累计32个不同序号的稳定样本 |
| 2 | RUNNING | elapsed增加；真实输入控制逻辑球位置 |
| 3 | PAUSED | elapsed和球位置冻结 |
| 4 | WON | 最终用时冻结 |
| 5 | ERROR | error=1关卡、2输入、3单次推进超100ms |

`[maze:imu] WHO_AM_I=0x6a result=0`为预期身份读数，**目前没有实际读到的证据**。代码沿用Lab/SDK寄存器方案，不凭编译结论断言板上精确型号。保持平放约1秒（首次目标32×20ms约0.64秒，不是实测承诺）；必须实际看到state从1到2。摇晃会重新累计，5秒未完成会进入输入错误。勿把这种失败删掉，重新平放后`maze_diag restart`重测。

每500ms一行诊断：a_mg为原始加速度换算到mg，g_mdps为角速度毫度/秒，bias_mg为校准偏置，tilt_milli为归一化输入×1000，pos_milli为球逻辑格坐标×1000，age为样本年龄ms。

依次**向屏幕右、左、下、上倾斜再回平**，每方向保留约2秒日志，说明当时实际倾斜方向。当前候选映射x=ax/y=ay，正x=屏幕右、正y=屏幕下；只有四向证据能确认是否需要换轴/反号。撞墙时位置不动不代表没有读数，先看tilt符号。

然后执行：

```text
maze_diag pause
maze_diag status
```

等约3秒，再发status，比较两个PAUSED的elapsed应相同；再执行：

```text
maze_diag resume
maze_diag status
maze_diag restart
```

继续后只累计RUNNING时间；重开run增加、回到起点/0ms并重新校准。重开时旧后台需要先停止/恢复配置，切换期间只等待新采样，不能吃旧局样本。最后：

```text
maze_diag back
```

应回READY并出现`[maze:imu] ... stopped result=0 settings_saved=1`；后者表示恢复写操作成功，不代替示波/实物验证。停止后再进入原Lab的IMU单次测试，检查可正常使用。若Lab原本忙，maze开始会进入输入错误并报`sampling slot unavailable`，不抢占已有作业；停止原测试后再restart。

请把真实串口原文保存到`evidence/M2/maze_adapter/board_日期_run01.txt`，另用文字说明板放置方向、四次倾斜时段和观察结果。有失败按run02等保留重测，禁止覆盖失败。到这一步必须暂停等真实证据，agent不会代替学生执行烧录或编造结果。

## 4. B 的实际接入接口

`maze_contract.h`仍为v1。B包含`maze_runtime.h`，不调用`maze_game_*`、不读controller私有字段，不创建第二个游戏timer，不做I2C。main已在Lab/LVGL初始化后调用`maze_runtime_init(selftests_passed)`；B不要重复init。

所有下列API只在main/LVGL线程调用；其他线程返回WRONG_STATE：

| 入口 | 语义 |
|---|---|
| `maze_runtime_attach(render, context)` | 绑定一个render回调；NULL为INVALID，重复绑定为WRONG_STATE。下一次20ms入口推送当前快照；每次只推变化revision。回调数据仅调用期间有效，B复制需要保留的数据。回调内不递归调用detach/action |
| `maze_runtime_snapshot(&snapshot)` | 复制快照，NULL为INVALID；不推进、不累计时间 |
| `maze_runtime_next_request(&id)` | 跨局递增非零编号，申请即消耗，失败动作也不重用；耗尽EXHAUSTED，不回绕 |
| `maze_runtime_action(NULL, &action)` | 可直接作为`maze_action_sink_t`。B携带**页面当前显示**的run_id和刚申请的id，禁止用新run自动重试旧动作。同步非阻塞。只接受本分配器已申请的编号 |
| `maze_runtime_detach()` | 先解除render/context；运行中结算并暂停，停止本会话采样，保留模型位置/时间/run及编号。再次进入attach后仍是PAUSED，等新样本后才允许继续；CALIBRATING重新累计窗口。先调用detach，后释放B对象 |

按钮示例（此函数在B的LVGL事件中调用）：

```c
maze_action_t a = { .action = MAZE_ACTION_PAUSE,
                    .expected_run_id = displayed_snapshot.run_id };
maze_result_t rc = maze_runtime_next_request(&a.request_id);
if (rc == MAZE_OK) rc = maze_runtime_action(NULL, &a);
maze_snapshot_t after;
maze_result_t read_rc = maze_runtime_snapshot(&after);
/* B按rc和after实际状态更新；失败也重读，不假定模型没变。 */
```

返回：OK=0、INVALID=-1、WRONG_STATE=-2、STALE=-3、EXHAUSTED=-4。暂停前结算可能先到WON/ERROR；START动作接受后也可能因后台工作槽忙进入ERROR，所以不能只看rc=OK就显示RUNNING。暂停/继续入口检查新鲜度，刚重新进入尚未收到样本时RESUME返回WRONG_STATE，保持PAUSED等待新样本。

返回按钮：先提交显式BACK并重读，成功后detach和销毁页面。普通对象暂时销毁/重建只detach，不隐式重置游戏。适配器timer是应用级唯一实例，不引用任何已销毁页面。B负责`maze_ui.c/.h`、静态布局、快照渲染、状态按钮和生命周期实机证据；A负责本模块及其真实输入验证。

## 5. 重跑主机验证

```powershell
cd D:\course_starter\build\maze-integration
uv run --no-project --with ziglang==0.13.0 python tests/run_maze_host.py --zig
```

也可用原生GCC/Clang：`python tests/run_maze_host.py --cc gcc`。不依赖WSL。逻辑52项使用生产C源码；另4项使用生产runtime.c及RT/LVGL/传感器传输替身，验证唯一线程/定时器、退出重进、槽归属和队列有界性；替身不验证实际I2C/LVGL时延。原始输出按真实执行时间自动另存，不覆盖旧失败。


## 2026-11-07 run05之后：先确认上沿压低

最新右沿压低已确认，但日志向左，方向仍FAIL；前面的软件修复记录不是物理验收结果。暂时保持当前固件，不需再烧录。屏幕正面朝上、文字正向，平放发`maze_diag restart`，手离开等6秒；随后把屏幕上边缘压低（或垫高下边缘），保持约2秒再平放。整段日志复制回传，不用实时盯state或cal。预期主tilt_y为负，但由实际原文判定；在取得该证据前不再次切换轴符号，也不宣称第五步通过。最新证据见evidence/merge-2026-11-06/board-run05/analysis.md。


## 最新可烧录版本：x=+ay，y=-ax

run05/06单方向取证后仅修正横向，58项主机测试及SDK最终编译通过。三镜像指纹见evidence/merge-2026-11-06/board-run06/software/firmware.json；此前“保持当前固件”仅适用于采集run06阶段，现在需要重烧本次生成的固件。

关闭串口监视器让出COM端口，在已激活SDK的终端进入D:\course_starter\build\maze-integration，运行`.\project\build_sf32lb52-lchspi-ulp_hcpu\uart_download.bat`。重连1000000 baud/CRLF，板放平，发`maze_diag restart`后手离开等6秒；按右沿→左沿→上沿→下沿依次压低（更靠近桌面），每次约2秒并回平约2秒，始终保持屏幕文字正向。整段日志直接回传，无需实时读数；预期x正/x负/y负/y正。完整四向及第五步必须以新证据判断，静态预览球不属于这一步的真实输入显示。
