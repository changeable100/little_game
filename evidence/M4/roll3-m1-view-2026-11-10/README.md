# TC-516 M1 斜俯视与慢移边缘

2026-11-10，负责人反馈 M1 试玩版赛道看起来正对镜头，运动边缘恢复像素级跳动。核对旧 M1 的场景坐标与相机基相同，近端平台侧面视角受限。游玩仍用 240×200 中心采样，缓慢移动时高对比边缘会跨过像素中心。两项判断来自旧版主机图、源码及同轨迹实验。

- 把 M1 赛道相对固定镜头偏转 30°。场景几何、检查点和回放共用场景坐标，镜头朝向和倾斜映射沿用现有实现。
- 游玩保留一次深度渲染，用两行已分配的工作区对高对比像素做 RGB565 轻量混色。暂停、开始等静止画面保留四点覆盖。
- 136 项组合核心回归通过，包括接缝、下坡通关、输入、复活，见 [核心回归](core-first.txt)。新增的渲染测试确认起点真实平台顶面与两个侧面可见。
- 在同一新版角度上比较同一段慢移，过滤前后的帧间绿色通道平方差为 1,215,047 与 378,350，降低 68.9%。两轮主机平均耗时比为 1.29 和 1.31；两种内存档逐像素一致，哨兵完好。[首轮](edge-first.txt) · [重测](edge-final.txt) · [慢移动图，左原采样右新滤波](slow-motion-before-after.gif)。
- 试玩源码在 D:/course_starter/build/m1-view-20261110。相对 B 4ae63b2 加 A 的 H1／自动回点组合基线，[修复补丁](m1-view.patch)已在原组合源码运行 git apply --check。正式合入时以该组合基线为前提。

## 主机图与烧录
[起点](M1-S.png) · [检查点](M1-C1.png) · [第二转角](M1-C2.png) · [终点](M1-G.png)。
[SDK LVGL](view-first.txt) 七态、控件与 20 轮通过；[SDK 编译](sdk-first.txt)通过，main.bin 1,231,472 字节，[镜像与源码指纹](firmware.json)。

USB 连板，关闭串口助手，在 PowerShell 执行：

~~~powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy RemoteSigned -Force
$env:Path = "$env:USERPROFILE\.local\bin;$env:Path"
. 'D:\SiFli-SDK\v2.5.1\export.ps1'
Set-Location 'D:\course_starter\build\m1-view-20261110'
& '.\project\build_sf32lb52-lchspi-ulp_hcpu\uart_download.bat'
~~~

串口提示填 COM 后的数字。进 3D 挑战，在首段绿坡轻倾慢滚，观察坡边与格纹，再通过紫色检查点与第二转角。如能同时提供 1000000 波特率下的 [roll3:frame] 行，可区分像素变化与实际帧间隔。

这一步需要你在真实硬件上验证并回传证据（日志/照片/截图，存进 evidence/），我需要看到证据才能继续。
