# 近景跟随与宽平台赛道（2026-11-09）

REQ-505 / TC-505，基于62d1418的工作区版本，尚未提交。主机图片是合成预览。core-first/retry为Zig沙箱访问失败；core-final为首次通关测试FAIL；core-retest为114项PASS。view-first/final为真实SDK LVGL主机测试；sdk-first/final为构建输出。没有本轮新固件板端PASS。

## 本轮烧录和一次验收
沿用 ../p2-integration-2026-11-09/README.md 的PowerShell环境准备/COM确认/烧录步骤（原补充完整保留）。本轮镜像应按**本目录firmware.json**核验，旧P2指纹不再匹配新构建。

1. 激活SDK，工作目录D:\course_starter\build\maze-integration，用本目录firmware.json的images路径和sha256核验镜像；运行 project/build_sf32lb52-lchspi-ulp_hcpu/uart_download.bat。
2. 1000000波特率记录完整串口后复位，确认roll3自检summary PASS failures=0及boot ready=1；进入3D挑战并平放开始。
3. 拍同一段视频：起点球路是否看清、右/上倾斜方向、沿坡道向右过检查点和桥，到转角向上，再向右到绿色终点。途中暂停/继续，掉落后回检查点，最后返回再进。掉落态镜头回看检查点是设计行为。
4. 将serial.txt和视频（大文件提供链接）/关键照片放本目录board-run01，反馈清晰度和是否感到卡顿；不必卡秒。失败保留原始日志。

编译成功仅证明可构建；板端帧时、操控和完整TC-504/505必须凭真实材料验收。
