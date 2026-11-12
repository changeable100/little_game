# TC-520 M3 细长回旋弯

负责人看过上一版双侧 U 槽的主机图，反馈弯道显得太宽、太厚，希望缩小体量并拉长行进路线。没有新版实机素材。本版维持固定斜俯视及两边高、中间低。

- 弯道全宽 2.56m→1.92m。平面向前伸出 55%，按平面中线积分，实际弯中路径约 6.68m→8.61m。拉伸时校正横向宽度，弯中也保持 1.92m。
- 弯中边缘抬升 0.50m→0.26m。弯道及两段连接坡侧壁变薄，沿行进方向每两段共用一格地砖颜色。进出口两段坡道与窄槽共边。
- [核心回归](core-first.txt) 147 项通过，含弯道实际尺寸、四向越缝、两侧自然回落、全关通关和自动回点。[36 帧绘制](edge-first.txt) 在两内存档逐像素一致。
- [修改补丁](m3-slim.patch) 以 D:/course_starter/build/m3-trough-20261110 为基线，新版位于 D:/course_starter/build/m3-slim-20261110。

[最终预览](M3-SLIM-before-after.png)左宽版、右窄长版。[最终主机回归](core-final.txt) 147 项、[LVGL](view-final.txt) 20 轮及 [串行绘制重测](edge-retest.txt)通过。并行编译期间的 [首次最终画面测试](edge-final.txt) 在旧 M1 性能判据失败，单独重跑后比值 1.29，保留原输出及原阈值。

## 烧录

[最终 SDK 编译](sdk-final.txt)通过，main.bin 1,250,632 字节。[镜像指纹](firmware.json)。

~~~powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy RemoteSigned -Force
$env:Path = "$env:USERPROFILE\.local\bin;$env:Path"
. 'D:\SiFli-SDK\v2.5.1\export.ps1'
Set-Location 'D:\course_starter\build\m3-slim-20261110'
& '.\project\build_sf32lb52-lchspi-ulp_hcpu\uart_download.bat'
~~~

先关闭串口助手，提示时填 COM 后的数字。真机观察窄槽的入弯、两侧回落及出弯，再到旗门。
