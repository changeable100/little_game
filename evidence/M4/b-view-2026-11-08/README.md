# B三维视图首批软件验证（主机合成，2026-11-08）

基线09ca370368a8907bf4b8d0f02ea8d122c35b212a，分支codex/roll3-b-view；用户确认推荐相机与首批范围。对应REQ-501 / TC-501；交接细节见docs/guides/B真三维视图首批交付.md。本目录所有PNG为真实SDK LVGL渲染的合成快照，不是实物照片。没有接入三维生产入口，不表示板上已能玩3D。

## 最终结果

| 证据 | 结果与范围 |
|---|---|
| host-view-final.txt | PASS：相机中心/方向/近面数学4项；透视大小、四元数条纹、共享坡道axis、墙体遮挡、近远/屏外裁剪哨兵、无效枚举/版本/半径/世界；7态中文/字体/禁用按钮、申请失败、20轮循环 |
| roll3-*.png / contact-sheet.png | 390×450主机七态，已逐态实际查看；文字完整、控件可见，READY/CALIBRATING为0秒，运行后合成12秒，掉落球低于平台，通关球位于终点平台 |
| core-host.txt | 原81+三维27+main隔离1=109项核心回归PASS；完整原始日志另见文件内标明的evidence/M2/maze_adapter路径 |
| 2d-connection.txt | 真实maze_runtime/controller/model与SDK LVGL生产连接回归PASS，RT/传感器为替身；启动门控、准备超时/重载、迟帧暂停/恢复、动作/退出/20轮保持 |
| renderer-compile-retest.txt / renderer-stack.txt | GCC14.2.1 -Wall -Wextra -Werror严格编译PASS；独立-O2函数栈报告，非真实线程栈高水位 |
| font-check.txt / assets/roll3-font | 201字符×16/20px字体可复现PASS；主机实际标签逐字符检查无placeholder，来源/OFL许可齐全 |
| sdk-build-final.txt | 原课程build_firmware.py指定本仓库构建，SDK2.5.1/GCC14.2.1/sf32lb52-lchspi-ulp PASS；main.bin1157512字节 |
| firmware.json | 三镜像SHA256、源文件和证据指纹；二进制留在忽略的project/build_*，不入库 |

20轮共140次有效七态渲染，另20次无效四元数错误显示；每轮交替本地返回与外部页面删除，重复destroy安全。每轮视图115200字节缓冲、对象、timer回基线；每次render不重新申请大缓冲。通用LVGL draw-buffer钩子观察额外分配0次。没有全堆监测或板端图形后端性能结论，不将这个结果扩写成整机无泄漏/FPS通过。

## 失败与修正保留

- host-view-initial.txt：受限进程启动本机老GCC报WinError623，未执行用例；允许本地主机工具执行后retest1通过。
- host-view-retest1/retest2.txt：真实LVGL首轮通过；后续改用静态标签文本，并让合成READY/校准计时归零，保持判据，不覆盖早期结果。
- sdk-build.txt：首个编译错误为缺少lv_image_cache_drop声明；SDK lvgl.h未导出该声明。显式包含SDK已有`src/misc/cache/instance/lv_image_cache.h`，未猜API或改SDK配置，sdk-build-retest通过。
- renderer-compile.txt：额外严格审计发现ARM无符号enum与零下界比较触发-Wtype-limits。改为无符号上界检查，同时拒绝负枚举并加拒绝用例；不放宽合法值范围，renderer-compile-retest通过。
- 最终SDK仅重编改动模块并保留RWX警告；retest完整构建另有newlib未实现系统调用/entry警告，均未屏蔽。没有删除功能以通过编译。

日志只规范生成输出的行尾空白，错误/诊断/测试内容保持；没有用户原始附件被改动。本批未修改A契约/物理/自检、二维模型/runtime/menu/main、SDK工程骨架或课程检查器/配置，详见source-boundary.txt。

## 复现

主机UI使用MinGW64 GCC4.9.2与SDK真实LVGL（CC设为编译器绝对路径）：

```powershell
python tests/render.py --sdk D:/sifli/SiFli-SDK/v2.5.1 --roll3-view --out build/roll3-view
python tests/render.py --sdk D:/sifli/SiFli-SDK/v2.5.1 --runtime-ui --out build/roll3-2d-regression
uv run --no-project --with ziglang==0.13.0 python tests/run_maze_host.py --zig
uv run --no-project --with pillow==12.3.0 --with fonttools==4.68.0 python tools/generate_maze_font.py --sdk D:/sifli/SiFli-SDK/v2.5.1 --roll3 --check
```

SDK环境激活后按原课程脚本build_firmware.py构建；本仓库为集成副本，实际运行忽略build/course_bridge.py把原脚本ROOT绑定到本仓库，未更改检查器。指纹记录本批来源/测试脚本，移交时可直接在标准课程布局使用原脚本。课程检查结果在course-checks.txt / trace.txt，历史缺口如实保留，未宣布M3/M4整机验收完成。

本轮严格追踪23条需求PASS。原check_all仍报8条历史署名判定、14天提交断档FAIL及manifest四缺项WARN；没有伪填学号/组号/会话路径或重写历史消除告警。两份课程格式提交正文在build/ROLL3-B-提交正文-文档.txt与build/ROLL3-B-提交正文-实现.txt，负责人随后已读并明确确认分两次本地提交，不推送；实际保存编号以Git历史为准。
