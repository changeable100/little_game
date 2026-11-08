# P0迟帧容错：烧录验收与B接口

2026-11-08；负责人王若豪（A）。分支`codex/maze-3d-physics`，开发起点`49dda19`。本批是已确认真3D方案A的第一步，仍显示现有二维迷宫；三维物理和独立物理任务尚未接入。A/B共享仓库尚未发布本批提交号，不能让B拉取一个不存在的提交。

## 这次行为变化

| 场景 | 当前行为 |
|---|---|
| 有效新采样，推进间隔不超过250ms | 按5ms小步补算，最多50步，余下0～4ms留给下次；104/108ms不再直接结束本局 |
| 推进间隔超过250ms | 保留run、球位和已完成用时，速度归零，进入已暂停；显示“迟帧已暂停，可点击继续” |
| 延迟暂停后继续 | 等待暂停之后发布的有效新采样，再手动点“继续”；不追赶暂停期间时间，不重开本局 |
| 输入不新鲜、非法值或未完成校准 | 保留原输入检查；100ms采样新鲜度、32个稳定样本和5秒校准期限未放宽 |
| 新局/返回 | 清空本局中断诊断；重开重新校准，返回按原规则结束采样并恢复配置 |

100ms原来是项目的时序保护要求，并非硬件超过100ms就不能运算。本次按负责人明确要求修订控制器策略；基础`maze_game_step`单批最大100ms仍保留，由控制器分批调用。250ms是允许补算的上限，不能当作操控流畅的目标。P0使用最新输入近似整个补算区间，不能还原中间每个姿态；后续独立物理任务会减少显示阻塞的影响。

## 烧录

先断开VS Code SiFli串口监视器及其他串口工具，释放端口。PowerShell执行：

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy RemoteSigned -Force
$env:Path = "$env:USERPROFILE\.local\bin;$env:Path"
. D:\SiFli-SDK\v2.5.1\export.ps1
cd D:\course_starter\build\maze-integration
.\project\build_sf32lb52-lchspi-ulp_hcpu\uart_download.bat
```

按设备管理器实际端口填写数字；此前可用CH340为COM3，仍为COM3才填`3`。出现“拒绝访问”先检查监视器是否占用，不靠关闭系统防护解决。使用本工作副本的生成脚本与地址，不使用外层旧工程镜像。指纹见`evidence/M4/p0-delay-2026-11-08/firmware.json`。

下载成功后重新连接串口，1000000 baud、CRLF；用实体复位或串口`reboot`启动，保存完整启动输出。预计模型24、关卡4、控制器31项板上自检全部PASS及菜单正常启动；这是新镜像的预期，不是已拿到的板测结果。

## 真机怎么验，不需要卡秒

1. 在首页进入游戏，板子放在稳定桌面上，点“开始”；等准备提示结束后发送一次`maze_diag status`即可。确认日志出现`state=2 error=0 cal=32`；不用在它出现瞬间输入。
2. 正常倾斜游玩，观察球能移动、中文显示正常，保存一段日志和照片。点击屏幕暂停，慢慢读两组日志；`state=3`且elapsed不变，`pause=1`。再点继续，仍同一run且用时继续。
3. 如果实际出现“迟帧已暂停”，保留现场，慢慢发一次`maze_diag status`，保存`timing/loop/pause`冻结记录及`diag/imu/gyro/input/clock`。预期`state=3 error=0 pause=2`，interrupted非零。放平、等一会儿后点“继续”，应同一run恢复移动；不要先重开把现场清掉。
4. 如出现输入异常，保留原错误日志，放平后按“重新加载”重新校准；输入错误不属于此次被放宽的时序条件。最后测试重开与返回：新run、重新准备，返回首页停止采样。

无需人为制造精确104/250/251ms卡顿，也不要暂停调试器/拔线来凑现象。确定边界由板上纯逻辑自检和主机注入覆盖；如果本轮没有自然遇到严重迟帧，就如实记“严重迟帧实物分支未触发”，不能以主机截图代替。

将日志、照片和实际操作顺序存入`evidence/M4/p0-delay-2026-11-08/board-run01/`；失败重测用run02另存。未提供的版本/精确时刻不猜测。收到证据后A追加manifest物理结论并单独准备hw提交。

这一步需要你在真实硬件上验证并回传证据（日志/照片/截图，存进 evidence/），我需要看到证据才能继续。

## B现在需要遵守的契约变化

`MAZE_CONTRACT_VERSION=2`，在`maze_snapshot_t`末尾新增：

```c
maze_pause_reason_t pause_reason; /* NONE=0, USER=1, DELAY=2 */
uint32_t interrupted_ms;         /* 本局未模拟的严重中断时长，饱和累计 */
```

坐标仍为二维格单位、左上原点、x右y下、半径0.20格；不要把此版本当作三维XYZ接口。所有相关C文件必须重编，不能链接旧v1对象。状态号与操作号未变。`elapsed_ms`仅计已完成RUNNING模拟，按5ms增长；小于5ms余量跨回调和手动暂停保留，严重卡顿则连同未模拟区间列入interrupted_ms。等待/校准/暂停不计有效用时。新局清零中断计数。

本批仍由主/LVGL线程独占runtime/controller；`maze_runtime_action/snapshot/attach/detach/next_request`均不可从新的工作线程直接调用。刷新timer归A管理，B不能新增一个物理推进timer；render收到的快照和level只借用到回调结束。B沿用现有生产adapter，点击前取当前run和新request，返回后重读快照，包括失败返回，不能由按钮猜测状态。

动作返回仍为OK(0)、INVALID(-1)、WRONG_STATE(-2)、STALE(-3)、EXHAUSTED(-4)。延迟暂停前没有新采样时继续可能返回WRONG_STATE，留在暂停等待再点击；不自动重发或伪造成功。用户PAUSE在结算时发现严重中断，可以返回OK并落在DELAY暂停，UI必须以重读快照为准。重复USER暂停仍WRONG_STATE。BACK只在成功后释放页面，旧页面/旧run事件仍拒绝。

A已更新延迟暂停中文提示及字库，B无需重新实现P0提示。B可先准备短赛道视觉草图和低多边形场景预算；等待A提供后续独立三维契约后再写renderer，不私自复用二维ball_x/y作为XYZ，也不启动第二路IMU。后续A负责球体接触/摩擦/时间与消息所有者，B负责固定视角3D画面、关卡表现和中文交互；真实手感由王若豪上板确认。
