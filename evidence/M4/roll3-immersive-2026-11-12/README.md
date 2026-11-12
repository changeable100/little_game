# 满屏游玩、暂停菜单与自然滚动结算（REQ-524 / REQ-525）

会话2026-11-11开工，跨日至11-12收尾；源码版本名和串口标记沿用开工日期，不伪造日志时间。完整源码/工程/测试/授权字体资源：`D:/course_starter/build/m3-immersive-20261111`。基线为完整m3-finish。Git过程文档仍在 `build/maze-integration`，其src尚未集成最新完整关卡；旧快照、补丁、实拍和工作区改动保留，未提交或推送。

## 负责人已确认的交互

- 首次踩绿锁定耗时，球继续自然滚动，离开绿色不撤销结算；约800ms上移、尺寸变化并淡入面板。面板有耗时、退出、下一关。下一关本轮明确置灰，后续单独设计，未把重开或历史赛道伪装为下一关。
- 画面铺满390×450，普通游玩只显示圆润计时牌和右上菜单。校准、准备好了、错误等为浮层；旧上下状态栏和底部按钮条移除。
- 菜单发送真实暂停并等待确认；确认前显示“请稍候”且不启用继续/回点。继续恢复手动暂停前运动，回存档点保持本局计时/掉落数，未激活检查点则回起点；退出关闭页面。严重迟帧恢复仍安全停止。

结算物理仍归原5ms任务：M3可选WON后继续重力/接触/滚阻，耗时保持；停止IMU输入，滚落不触发自动复活或抹除成绩。镜头锚定绿色区域，菜单和动画只使用既有页面刷新。旧M1/M2获胜仍停球。结算后的采样年龄可正常增长，不意味着运行中的100ms输入保护已放宽。

竖屏200×232、2倍输出400×464居中微裁，实际覆盖全屏。颜色92800字节；常规/低内存两档总计142544/122544字节，均小于前版153600/129600。面板逐项绘制属性淡入，避免对整卡做透明变换所需的大型临时ARGB层。主机绘图缓冲钩子新增申请为0；这不是板端全堆高水位/FPS证明。

## 验证与失败保留

- `core-before.txt`：旧实现对9点继续滚动、滚出不撤销、无IMU推进、暂停恢复运动、暂停回点共5项FAIL。
- `core-final-retest.txt`：162条核心TC PASS。包含上述同源启动自检、旧M1/M2/M3通关/弧槽/护栏/高速四项、输入/迟帧/复活保护，以及真实worker停止IMU后仍推进背景并正确关闭。新worker测试首次误以为stop请求会同步清掉测试替身的held_token，在`core-final.txt`保留FAIL；按该套既有异步确认模型先验证停止请求，再完成确认、验证释放和运动，未降低应用要求。
- `view-complete.txt`：真实SDK LVGL满屏/HUD、校准与准备弹窗、真实动作发送/暂停确认/继续/回点/重新校准、0/200/400/800ms位置/尺寸/透明度、重复快照不重启、冻结耗时、下一关禁用、动画中销毁/重入/回绕、七态字形及20轮资源回收通过。普通渲染与生产动作使用真实C模块，传感器/RT在主机测试中为替身。
- `view-stale-before.txt`：补测复现隐藏旧菜单“回到存档点”按钮的迟到CLICK被当前分支当成退出。已增加真实按下/点击、局号与可见祖先校验，切换浮层清按下状态，隐藏按钮禁用；补测及完整UI回归通过。测试点击按实际LVGL流程发送PRESSED再CLICKED，单独迟到事件被拒绝。
- `view-strict-final.txt`：最终视图-Wall/-Wextra/-Werror编译成功，成功无输出；实际命令和退出码记录于会话与最终审计。
- `edge-first.txt`：200×232的100/50行尾带逐像素一致、密集覆盖46400/46400、哨兵与原质量阈值通过；慢移帧间差降低约68.8%，滤波/原绘制耗时比约1.20。M2/M3各36帧通过。`edge-probe-final.txt`：800组无预算耗尽，400个隔离开放边无异常速度突降。整关真实障碍/落地的大减速不等同开放边失败。
- 字体`font-generation-local.txt`/`font-check.txt`：SDK授权Source Han Sans、固定Pillow12.3.0/fontTools4.68.0，195字符×16/20/28px，全部覆盖。最初默认Python缺fontTools，uv离线解析也因注册表缓存不完整失败；复用已存在的精确版本包目录完成，未改系统字体或访问网络。生成器/来源指纹/OFL在完整目录tools/assets中。
- `sdk-first.txt`及`sdk-final.txt`：原课程build_firmware入口，SDK2.5.1、sf32lb52-lchspi-ulp编译成功，SDK既有告警保留，无新roll3告警。最终main.bin 1,313,656字节，SHA256 `b3351d4c291309dbbb105ea2890d0e6f8f41ee6ed3b2f18ab299f09fe38404d8`。

`IMMERSIVE-gallery.png`为游玩/菜单/结算，`IMMERSIVE-result-animation.png`为四个动画时刻；均从实际SDK LVGL渲染生成，已查看，不是板测或AI概念图。`firmware.json`保存三镜像、源码/字体资源/工程和补丁指纹。`m3-immersive.patch`以完整m3-finish为基线，正向/逆向检查通过，旧固件仍保持原0d193b19…指纹，工程骨架/配置及src/SConscript未改。

REQ-521的25%主机绘制目标仍为历史未完成项；新纵向画面没有被拿来改写旧对照或冒称板端帧率通过。完整头文件/实现/字体需同步重编，不能混用旧世界结构的对象文件。

## 烧录及新证据

后续2026-11-12已收到两张真实照片，原图与限定分析在board-run01。支持游玩HUD与结算页面显示；负责人不接受镜头大小及控件风格，选择A粉紫像素风继续整改。没有动态/串口或独立镜像指纹，整项未验收；下面保留原交付时的烧录说明，新候选见roll3-pixel-2026-11-12。

收尾审计：179项源码/镜像/工程/补丁指纹一致，旧固件保持，受保护路径无变更，docs/manifest的diff检查通过。44条严格需求追踪与提交正文草稿校验通过。原课程check_all仍因8个历史提交不合规和14天历史提交空档失败，manifest另有4项待负责人填写，未改写历史或编造身份；详见course-checks.txt、trace-final.txt、commit-message-check.txt与final-audit.json。

新UI/结算尚未烧录验收。关闭串口助手，USB接USB-to-UART，提示时输入COM后的数字：

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy RemoteSigned -Force
$env:Path = "$env:USERPROFILE\.local\bin;$env:Path"
. 'D:\SiFli-SDK\v2.5.1\export.ps1'
Set-Location 'D:\course_starter\build\m3-immersive-20261111'
& '.\project\build_sf32lb52-lchspi-ulp_hcpu\uart_download.bat'
```

串口1000000，入口应显示`build=m3-immersive-20261111`。请回传一段包含放平校准/准备、满屏慢滚、菜单暂停→继续/回点、触绿后球继续运动和0.8秒结算的实拍；日志保留启动TC-524/525、状态和帧诊断。证据放本目录board-run01；旧视频不替代新UI验收。

这一步需要你在真实硬件上验证并回传证据（日志/照片/截图，存进 evidence/），我需要看到证据才能继续。

重现：在完整目录使用安装Zig的Python运行`tests/run_maze_host.py --zig`、`tests/run_roll3_sdk_view.py`；既有环境为`C:/Users/10436/AppData/Local/uv/cache/archive-v0/BwMOkx4NJxyDmgJP/Scripts/python.exe`。边缘/冲边按日志中的Zig cc命令链接对应tests文件与roll3_scene.c/roll3_physics.c。字体精确包目录和命令见本会话与font日志，--check保持生成器版本锁。提交草稿另备，须负责人实际读过确认后才能集成保存。
