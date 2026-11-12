# TC-515 自动返回检查点

2026-11-10，A 分支 codex/roll3-downhill-a，提交基线 1ab64e2。负责人报告 H1 可玩、当前手感没有问题。视频能看到游玩和掉落，实拍证据见 [H1 板测](../roll3-h1-2026-11-10/board-run01/analysis.md)。

## A 侧验证

- [129 项核心回归](core-first.txt)通过。新增 TC-515 覆盖 599ms/600ms 边界、检查点 0 与 1、计时及次数、时钟回绕、新鲜样本、手动提前回点、页面心跳、重开和退出优先。
- [SDK LVGL 主机预览](view-first.txt)通过，涵盖七种状态、控件和 20 轮页面开关。
- [SDK 编译](sdk-first.txt)通过，板型 sf32lb52-lchspi-ulp，main.bin 1,229,320 字节。[三镜像指纹](firmware.json)对应 A 分支现有关卡、H1 和自动回点。

- [课程检查](course-checks.txt)的需求追踪通过。总检查仍报告 8 个历史提交格式问题、14 天提交间隔和 manifest 4 个待补字段。

## B 推送复核

B 分支 codex/roll3-downhill-b 的 [4ae63b2](https://github.com/changeable100/little_game/commit/4ae63b221e521975d35b627c38eeb98f05c937fb) 与 A 的共同代码基线为 d422c3c。M1 由三段缓坡、两个转角和一个检查点组成，高度由 1.50 降到 0.10，固定斜俯视角保持。B 的回放记录为 32.28 秒通关，检查点 1，掉落 0 次，属于合成输入。

独立副本中 [B 原版 129 项](b-core.txt)与 [叠加 A 的 H1 和自动回点后 136 项](ab-core.txt)均通过，包括真实 M1 场景中的检查点触发与自动回点。同一编号曾被 A、B 分别用于 H1 与 M1，A 的 H1 文档改为 ADR-0013。组合验证的范围是源码和核心测试，A 的可烧录固件使用现有地图。后续正式合入时需协调两边修改过的需求、设计、测试和 manifest。

## 真机验证

旧地图真机反馈已确认。视频支持检查点 1 的自动返回、恢复游玩及掉落计数，证据见 [实拍判读](board-run01/analysis.md)。检查点 0、计时与 600ms 边界仍以主机回归记录为准。以下保留本版的烧录与复测步骤。

用 USB 连接开发板并关闭串口助手。新开 PowerShell 执行以下命令，批处理询问时输入 COM 后的数字。

~~~powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy RemoteSigned -Force
$env:Path = "$env:USERPROFILE\.local\bin;$env:Path"
. 'D:\SiFli-SDK\v2.5.1\export.ps1'
Set-Location 'D:\course_starter\build\maze-integration'
& '.\project\build_sf32lb52-lchspi-ulp_hcpu\uart_download.bat'
~~~

1. 校准并开始 3D 挑战，未到检查点时掉落。松手回平，约 0.6 秒应从掉落画面返回起点，掉落数加一。
2. 到达检查点 1 后再次掉落。约 0.6 秒后应回到检查点，保留本局计时和掉落次数。
3. 将视频或 1000000 波特率串口日志存入本目录的板测证据子目录。日志应各有一行 [roll3:respawn] auto，分别带 checkpoint=0 和 checkpoint=1。

这一步需要你在真实硬件上验证并回传证据（日志/照片/截图，存进 evidence/），我需要看到证据才能继续。

## M1 试玩固件

B 的 M1 场景加上 A 的 H1 与自动复活，独立目录 D:\course_starter\build\m1-preview-20261110。与上轮组合主机测试所用的 111 个工程和源文件一致。[SDK 编译](m1-preview-sdk.txt)通过，main.bin 1,231,072 字节，[三镜像指纹](m1-preview-firmware.json)。旧地图的固件仍可按上面的路径使用。

烧录步骤沿用上文，唯独将 Set-Location 改成 D:\course_starter\build\m1-preview-20261110。进入 3D 挑战，看第一段下坡与紫色检查点，经过蓝色转角和最后一段绿坡到金色终点。要复测组合行为，可在紫色检查点激活后掉落一次，记录自动回点和后续通关。M1 真机效果待板测。
