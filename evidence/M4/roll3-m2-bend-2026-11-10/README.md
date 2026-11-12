# TC-517 M2 下坡回转试玩

参考用户提供的《11月10日.mp4》，14.81 秒，原文件 SHA256 A407926A77DCF365AF4CFD5E07299F7F29B5D74912CB6B69B8A0C05CFDB647D0。视频用于关卡设计，M2 实机运行另需证据。负责人在 A 分段宽平台和 B 连续弧形槽之间选择 B。

高台起点 3.20，短缓坡后在宽平台回平转向，长坡落到 1.55 的紫色检查点。半环弯中心线从 1.25 降到 0.86，弯中外侧抬高，外挡边随曲线延伸。出弯短上坡接到 1.08 的金色终点。沿用固定斜俯视、游玩边缘平滑和自动复活。

[起点](M2-S.png) · [长坡](M2-RAMP.png) · [检查点](M2-CP.png) · [弧形槽](M2-BEND.png) · [终点](M2-G.png)

## 主机验证

- [最终核心](core-final.txt) 141 项通过，含两处四向越缝、外挡边、M2 连续通关及检查点自动回点后续跑。首次及首次重测的双向测试失败保留在 [首轮](core-first.txt)、[重测](core-retest.txt)。诊断显示球已过缝并接地，但逆行达不到固定时限内的目标位移。给测试输入补坡度前馈并匹配接近速度，越缝判据和赛道实现保持；[方向诊断](crossing-debug-retest.txt) 为过程记录。
- [真实 SDK LVGL](view-retest.txt) 七态与 20 轮页面开关通过，输出五张 M2 预览。首次预览缺测试头文件，[失败记录](view-first.txt) 保留。
- [弧道 36 帧](edge-first.txt) 的 100 行与 50 行内存档逐像素一致，哨兵完好。主机弯道样本中心采样平均 0.899ms、含边缘滤波 1.128ms。真机耗时与输入感受单独测。
- [M2 修复补丁](m2-bend.patch) 以已出过试玩固件的 M1 斜俯视修正版源码为基线。M2 源码与固件目录为 D:/course_starter/build/m2-bend-20261110。

## 烧录与板测

[最终 SDK 编译](sdk-final.txt)通过，main.bin 1,238,760 字节。[三镜像及源码指纹](firmware.json)。

~~~powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy RemoteSigned -Force
$env:Path = "$env:USERPROFILE\.local\bin;$env:Path"
. 'D:\SiFli-SDK\v2.5.1\export.ps1'
Set-Location 'D:\course_starter\build\m2-bend-20261110'
& '.\project\build_sf32lb52-lchspi-ulp_hcpu\uart_download.bat'
~~~

USB 连板，先关闭串口助手，提示时填 COM 后的数字。进入 3D 挑战，从长下坡进紫色检查点，在弯里先慢滚一次再贴外挡边走一段，随后滚过小抬坡到终点。检查点激活后可以另录一次掉落回点。若能回传串口，波特率为 1000000，留存 [roll3:frame]。

这一步需要你在真实硬件上验证并回传证据（日志/照片/截图，存进 evidence/），我需要看到证据才能继续。
