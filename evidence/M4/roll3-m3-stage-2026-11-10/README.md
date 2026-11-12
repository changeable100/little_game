# TC-518 M3 层叠高台与完整下坡关卡

2026-11-10，负责人反馈 M2 玩着没有问题，关卡及画面仍与参考差异大。此前只复刻路线节奏，画面缺高台体积、层次和引导。负责人选择 A，同一版本改路线和场景。该反馈为文字，没有新的真机视频或串口。

## 地图与画面

起点和落脚平台有清楚的高差。高台顶面为粉白格纹，侧面延伸至下方。前段宽平台加错位挡边，引导先横移再转入下坡。长坡分为两段，中间是一块可回平的紫色平台。紫色检查点加金环与旗，尾段沿原连续 U 形槽绕回低处的双旗门。两块直道挡边及弧槽外缘参与碰撞。背景高台、柱体、旗和箭头为场景标记，路面和真实挡边由物理几何绘制。

[起点](M3-S.png) · [转向](M3-TURN.png) · [落脚平台](M3-LANDING.png) · [检查点](M3-CP.png) · [U 弯](M3-BEND.png) · [旗门](M3-G.png)

## 软件验证

- [核心回归](core-first.txt) 143 项通过。两块导栏挡住测试球，连续回放通过两次转向、坡间落脚、检查点、U 弯和终点，保留自动回点与 M1/M2 回归。
- [SDK LVGL](view-final.txt) 六处主机截图已逐张查看，20 轮页面开关通过。两种工作区内存档输出逐像素一致，画面工作区与像素分辨率沿用 M2。
- [最终绘制对照](edge-final.txt) 的 36 帧主机平均耗时，M3 同一几何去除装饰 1.220ms，带装饰 1.498ms，同轮 M2 弧道 1.297ms。厚侧壁初版见 [首轮](edge-first.txt)，随后按视角跳过背面，最终真机帧时待测。
- [源码补丁](m3-stage.patch) 以独立 M2 弧槽版为基线。固件使用独立 M3 目录，启动仍从「3D 挑战」进入。

## 烧录与板测

收到旧版 M3 的 [实拍记录](board-run01/analysis.md)，确认后半段回点和通关。新版 U 槽另测。

[最终 SDK 编译](sdk-final.txt)通过，main.bin 1,249,032 字节。[镜像指纹](firmware.json)。

~~~powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy RemoteSigned -Force
$env:Path = "$env:USERPROFILE\.local\bin;$env:Path"
. 'D:\SiFli-SDK\v2.5.1\export.ps1'
Set-Location 'D:\course_starter\build\m3-stage-20261110'
& '.\project\build_sf32lb52-lchspi-ulp_hcpu\uart_download.bat'
~~~

关闭串口助手，提示时填 COM 后的数字。进入 3D 挑战，按箭头过转向平台，试在紫色落脚平台回平，再经过检查点绕 U 弯进双旗门。如有串口可同时留存 [roll3:frame]，波特率 1000000。

这一步需要你在真实硬件上验证并回传证据（日志/照片/截图，存进 evidence/），我需要看到证据才能继续。
