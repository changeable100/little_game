# 完整源码发布整理

负责人2026-11-12要求将成果保存并推送。发布分支codex/roll3-release-20261112从1ab64e2建立在独立worktree，原build/maze-integration的292个修改/未跟踪文件和全部试玩目录保持，另备份至D:/course_starter/build/git-publish-backup-20261112（库存及binary diff见该目录）。初次worktree写Git元数据受沙箱限制，按任务授权提权成功；没有强制改写历史。

最新完整m3-pixel生产源码、关卡、会话/字库/测试/工具和既有过程文档、证据已集成。二维文件仅换行差异时保留现有文本，旧文件不删除。集成分支原缺失的课程AGENTS/CLAUDE、脚本、配置、钩子与指南从A目录原样补齐，指纹见pre-integration.json；没有改检查阈值。原有.gitattributes的证据原字节规则保留，备份脚本碰到与A规则不同后停止，核对后续作，不覆盖。

## 本次重新验证

- core-integrated.txt：原162项核心/运行时回归PASS。
- view-integrated.txt：真实SDK LVGL七态/20轮/菜单动作/旧事件/800ms结算/销毁重入与像素字形等PASS，绘图大缓冲新增申请0次（非板端堆/FPS）。
- sdk-integrated.txt：直接运行原python scripts/build_firmware.py，SDK2.5.1/sf32lb52-lchspi-ulp编译PASS。
- integration-audit.json：179项源码/工程比较无语义差异，三镜像均与原试玩候选完全同SHA256。main.bin1,244,176字节，aefb41ff3731fc25f60191a69d7ab99a17eb81a1ebb0ae1129beb1fee32fc56d。
- trace-integrated.txt：45需求严格追踪PASS。course-integrated.txt保留原8条历史提交不合规、14天提交空档FAIL及manifest4缺项WARN，未改历史或伪造身份。最终文件空白、原钩子消息检查和发布状态另存。
- 发布文件约21MB，无超过10MiB单文件；凭据模式扫描未发现疑似令牌/私钥。编译产物和原始视频仅留本地，Git保留截图/提取帧/日志/补丁/指纹。

## 提交与远程

已准备11条小步提交：原版课程工具、需求/设计/TC文档、M3高台、双侧U槽、细长槽、曲面速度修复、终点护栏、满屏动态结算、粉紫像素风、证据、发布记录。各阶段源码按已有试玩快照纳入真实当前日期的Git提交，不伪造历史开发日期；过去的失败与重测保持。正文和路径清单在commit-review.md、commit-plan.json和messages/。

负责人已实际阅读11条正文并确认提交/推送。已按原钩子逐步保存各快照与证据，记录在commit-records.json；本文件随最后交付记录提交。远端发布通过普通push并核对HEAD，不使用force或no-verify，最终状态见GitHub分支和本地推送回执。

负责人后续指定B创作下一批地图，明确通过本次GitHub推送交付说明。任务书在docs/guides/B后续地图创作与交付规范-2026-11-12.md，README与旧B任务书已加入口；B从最新发布提交开独立地图分支，A保留核心物理、UI与集成职责。未尝试向不明身份会话发消息。

新像素固件没有新增真机证据；原两张照片只能证明上版静态页面，视觉被负责人否决。新构图/字体/触摸/动画/球运动及流畅度仍待验证，旧25%绘制目标未完成。
