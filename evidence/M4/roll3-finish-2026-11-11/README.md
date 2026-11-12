# M3 弧槽微加深与绿色终点（REQ-522 / REQ-523）

负责人明确确认：最大横向凹深0.26→0.32m，整块绿色承载路面接触即锁定成功，三条开放边实体护栏，1秒后提示通关。完整源码/工程/测试/固件在 `D:/course_starter/build/m3-finish-20261111`。上一版m3-fluid和各试玩快照保持；Git工作区 `build/maze-integration/src` 仍未集成完整最新关卡，不要从那里编译本版。

## 结果与实现边界

- 弧槽同一剖面供绘制和碰撞，宽1.92m、拉伸1.55和两端收平保持，两缘相对中央改为0.32m。
- 绿色平台原来只有中心±0.42m区域判定，现改为真实支撑接触指定承载盒。9个中央/四角/四边测试点立即WON、清线速度和角速度、冻结用时；上方悬空、只碰侧壁或仍在粉色连接坡不会获胜。M1/M2保留原区域逻辑。
- 绿色平台三条无连接边各有高0.30m、厚0.08m的实体护栏，连接坡入口开放。OBB预算12→16，M3用15；持久画面缓冲仍153600/129600字节，不增加物理时钟或业务状态。世界结构尾部增加可选终点盒索引，整合时需同时更新头文件/实现并完整编译，不能混用旧世界结构的对象文件。
- 物理进入WON即停球。现有页面刷新在模型revision冻结后继续检查1000ms提示；等待只显示“请等待”，之后显示“通关＋冻结用时”。没有再等一秒才判胜、没有要求持续停留。重开请求等待处理时抑制旧提示，返回/销毁取消，重复WON不重置等待，32位毫秒回绕通过。

## 软件证据

- `core-before.txt` 保存旧版四条FAIL：0.32m、整块终点、入口边路和三护栏几何。旧SDK LVGL立即通关不符合延时要求，`view-before.txt`保留失败。
- `core-second.txt`：157条TC PASS。含6条新终点同源启动自检、0.32m剖面、原M1/M2/M3通关/两侧回落、原TC-521四条高速曲面、输入/迟帧/复活和预算保护。三护栏碰撞测试关闭获胜判定，在每边3处以2m/s撞击，避免用停球掩盖穿透；入口左中右可进入。
- `view-final.txt`：真实SDK LVGL七态/20轮资源释放、999/1000ms、冻结revision的生产页面刷新、重开待处理/取消、返回/新页面和回绕通过。等待文案首版带冻结秒数，可能被误解为等待时长，最终改为“请等待”并重测；原边界未放宽。新增断言最初按单空格写文字，实施时对齐既有双空格成绩格式；旧版实际为立即通关，未隐藏原失败。
- `edge-final.txt`：原边缘质量和M1慢移阈值、M2/M3各36帧、100/50行逐像素一致、哨兵通过。`edge-probe-final.txt`：800组无迭代耗尽，400组隔离开放边无突然损失超过25%；整关包含真实障碍/落地的大减速记录，不将其等同开放边结果。
- `sdk-first.txt`和`sdk-final.txt`：SiFli SDK v2.5.1 / sf32lb52-lchspi-ulp，通过原课程build_firmware入口编译成功。保留SDK旧警告，无新增roll3警告。最终main.bin 1,257,048字节，SHA256 `0d193b19635cd67718234127a5612573bf4f059a70cc06052e3c95c392b97ab3`。三镜像、源码、工程和补丁指纹见`firmware.json`。

`M3-BEND-compare.png`左0.26m、右0.32m；`M3-GOAL-delay-final.png`左接触锁定、右1000ms后提示，均为真实SDK LVGL主机模拟，已实际查看，不是实拍。`m3-finish.patch`基于完整m3-fluid；旧版正向、新版逆向应用检查通过，未实际修改旧快照。原m3-fluid镜像仍为原指纹，工程骨架/配置与src/SConscript未改。

REQ-521的25%主机绘制目标依然保留失败状态，本轮未重新选阈值或以新几何声称达标。33ms是刷新请求，实际FPS/整体流畅度待板端日志。

原课程new_devlog已刷新。严格需求追踪42条通过（`trace-final.txt`）；总检查仍为8条历史提交格式、14天历史间隔FAIL和manifest四缺项WARN（`course-checks.txt`）。未改课程检查器或受保护文件。提交草稿见`commit-review.md`，完整Git集成、提交及推送尚未执行。

## 前一版实拍与本版待验证

本次收到的`VID_20261111_222004.mp4`对应前版上下文，元数据、抽帧和限定分析在 `../roll3-fluid-2026-11-11/board-run01/`。视频支持后半段游玩、终点接近与片尾离道路；无独立指纹和串口，不是本版0.32m/护栏/新通关时序的验收证据。

本版尚未烧录，无新物理通过记录。请一次视频覆盖弧槽进出、绿色平台偏边接触后的停球、约1秒通关提示及重开/返回；若能抓串口，波特率1000000，保留启动TC-522/523和`[roll3:open] build=m3-finish-20261111`。证据存本目录`board-run01/`，收到后再追加manifest物理记录。

这一步需要你在真实硬件上验证并回传证据（日志/照片/截图，存进 evidence/），我需要看到证据才能继续。

## 烧录

关闭串口助手，USB接USB-to-UART口，提示时输入COM后的数字。

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy RemoteSigned -Force
$env:Path = "$env:USERPROFILE\.local\bin;$env:Path"
. 'D:\SiFli-SDK\v2.5.1\export.ps1'
Set-Location 'D:\course_starter\build\m3-finish-20261111'
& '.\project\build_sf32lb52-lchspi-ulp_hcpu\uart_download.bat'
```

主机核心和LVGL可在本版目录用安装ziglang的Python分别执行`tests/run_maze_host.py --zig`及`tests/run_roll3_sdk_view.py`；既有可用Python为`C:/Users/10436/AppData/Local/uv/cache/archive-v0/BwMOkx4NJxyDmgJP/Scripts/python.exe`。独立绘制和冲边测试使用同一Python的`-m ziglang cc -std=c11 -O2 -Wall -Wextra -Werror -Isrc`，输入`tests/roll3_edge_compare.c`或`tests/roll3_edge_probe.c`，并链接`src/roll3_scene.c src/roll3_physics.c`。前版的同快照旧/新性能对照夹具属于0.26m数据，不作为本次0.32m新关卡的性能结论。
