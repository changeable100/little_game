# A启动保障验证

基线398d725，2026-11-07。原始host_2026-11-07_220022.txt为中间68项PASS，220226为最终69项PASS，均在evidence/M2/maze_adapter。使用uv隔离Zig0.13/Clang18运行tests/run_maze_host.py --zig，RT/LVGL/采样传输为替身；main宿主测试调用生产入口。

build.txt为原课程构建脚本指向本工作副本、激活SDK v2.5.1后的实际输出；firmware.json记构建时未提交状态和三个镜像指纹，不是板端运行证明。工程配置和保护文件未改。新启动正常结果须用户另提供原文，失败分配不要求在真机注入。trace_before_report.txt保留报告未覆盖新增REQ-408时的严格检查失败。
