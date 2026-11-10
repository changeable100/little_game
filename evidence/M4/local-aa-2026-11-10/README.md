# REQ-509 / TC-509 局部四点抗锯齿
游戏已接入中栏方案。240×200中心样本找到轮廓和格纹边，向四周扩1像素。候选做四点独立深度测试；条带分批复用旧深度平面，页面持有201600字节缓冲，比单采样多9600字节。

## 主机结果
- comparison.png 使用与上一轮相同的36帧轨迹，同一机位及240×200分辨率，三栏用同样的主机缩放。host-*.png 为真实SDK LVGL页面合成截图。
- 520420个候选中520417个与独立整帧四点参考的各RGB565通道差不超过一级；所有非候选保持中心值。密集全遮罩48000个像素均通过，空遮罩及缓冲哨兵通过。
- 本次同一主机、36帧平均绘制耗时：中心0.437ms，局部1.411ms，整帧四点1.771ms。只统计渲染器，不含动画写盘、LVGL面板输出和真实设备调度。相对整帧四点约节省20%，相对原单采样增加计算。细线若被中心样本漏掉仍可能残留。
- compare-first 为逐像素跳过版本；compare-final 为逐字位图迭代版本。真实SDK LVGL七态和20轮进入/退出、119项核心回归通过。构建与课程检查原始记录同目录。

## 烧录与真机复测
沿用[COM与SDK准备](../p3-tuning-2026-11-09/README.md)，镜像核验文件改为 **evidence/M4/local-aa-2026-11-10/firmware.json**。运行 `project/build_sf32lb52-lchspi-ulp_hcpu/uart_download.bat`，1000000 baud、8N1收串口并复位。确认 `TC-509 edge-threshold-and-word-boundary PASS`、`TC-509 four-sample-resolve PASS` 及roll3启动自检通过。
在3D挑战里平放校准，固定拍摄位置，慢移经过绿坡与蓝平台，再经过较远格纹。拍清轮廓、格纹闪烁和操作反馈，串口保留连续数行 `[roll3:frame]`。重点读 `draw_max_ms` 和 `gap_max_ms`，与上一版同一路段数据比较。真机视频、日志进本目录board-run01。

## 重现主机对照
编译 `tests/roll3_edge_compare.c` 与 `src/roll3_scene.c`、`src/roll3_physics.c`，包含目录 `-Isrc`。传任一参数时，测试在当前目录输出36帧×3种PPM。主机动画在 `build/local-aa-2026-11-10/slow-pan.gif`，仅作同步慢移对照。
