# P2集成软件证据

2026-11-08，合并8318a18；提示修正构建时未提交。connection_before_fix.txt保留新增断言失败，after_fix/final保留重测，regression69与build.txt为本地原始执行输出。主机运行使用SDK LVGL和生产runtime/适配代码；本机Windows Zig --wrap兼容问题通过忽略build/check_p2_integration_ui.py的符号别名适配解决，不删除故障注入或断言。

host-*.png从本轮生成PPM转换，仅主机预览；没有本轮物理证据。三镜像指纹只证明构建产物，不能推断实际已烧录。板端步骤见docs/guides/P2整机上板验证-2026-11-08.md。
