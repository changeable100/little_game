# M3 构图缩小与粉紫像素界面

负责人根据上版两张真机照片选择A：场景缩小约25%，计时、菜单、校准/准备、暂停及结算统一粉紫像素风。原图/指纹/限定物理记录保留在../roll3-immersive-2026-11-12/board-run01。本版完整源码、测试、授权字库和工程位于 `D:/course_starter/build/m3-pixel-20261112`，基线m3-immersive。独立版本未覆盖任何旧快照或Git既有改动，完整新关卡尚未集成入Git。

## 已实现

FOCAL统一乘0.75，正常相同深度的水平和竖直离心投影均为上一满屏版75%，保留200×232、2倍显示、满屏和固定斜俯方向/跟随。正常球投影约32屏幕像素，仍>=24。物理/输入/迟帧参数、世界、结算自然运动及800ms时序保持。

UI采用粉紫/浅藕色、2px阶梯轮廓和4px实体阴影；计时缩为140×44，菜单仍56×56，主要按钮仍44px高。原生矩形回调绘制，无额外离屏画面或新timer，阴影位于控件边界内。字库为半尺寸单色栅格2倍扩大，原绘制细线转为2×2块；数字采用生成器内原创5×7点阵。中文提示采用10点阵20px，标题14点阵28px；16px仅供超长计时数字回退，不让普通中文使用难读的8点阵。既有Source Han Sans授权、源文件指纹和生成器版本锁保留。

`PIXEL-comparison-final.png`上排为旧、下排为本版同位置主机画面；`PIXEL-gallery-final.png`为本版游玩/暂停/结算；`PIXEL-states-final.png`含起点/长坡/弯中及校准/准备/动画中间帧。均由实际SDK LVGL渲染，已查看，是主机模拟而非实拍。画面常规142544/低内存122544字节，主机绘图大缓冲新增申请0次，不外推板端全堆高水位或FPS。

## 验证与失败保留

- camera-before.txt：旧倍率在新增水平/竖直75%断言均FAIL；相同断言在view-final.txt的同源renderer自检PASS，板上启动也会执行。
- view-before.txt：旧4bpp字体在新增单色2×2判据FAIL。font-generation.txt/font-check.txt及最终真实SDK测试验证196字×16/20/28、字形覆盖、每个2×2位块相等、偶数metrics通过。OFL与provenance在完整目录assets/roll3-font，具体包版本/缓存路径沿前交接。
- core-first.txt：原162项核心/运行时回归通过；物理/会话生产代码未改。
- view-first.txt：扩大视野后旧“离屏”夹具进入视野，原平移覆盖相等断言失败。view-retest.txt/view-retest2.txt保留把夹具先挪1000m导致超出现有100m世界边界的失败；改成合法80m远平面外夹具，维持原等量判据，未放宽验证阈值。新增配色断言按SDK ARGB取RGB，生产配色未因此改变。
- view-retest3.txt/view-final.txt：真实SDK LVGL满屏/七态字形/20轮进出及资源回收、动作请求/确认/重复/旧事件、继续/回点/重校准、800ms/冻结成绩/禁用下一关/销毁重入回绕均PASS。新增菜单面积/按钮高度/零原生圆角/粉紫/动画偶数坐标检查通过。实际初图小号中文难辨，后把提示和菜单改20px并留出徽章/标题间距，最终逐图核对。
- edge-final.txt：串行运行原阈值，46400/46400密集覆盖、100/50行尾带逐像素一致和哨兵，慢移能量降低68.8%、滤波/原绘制比1.22，M2/M3各36帧两档一致PASS。未用新构图改写旧REQ-521的25%性能目标，该目标仍未完成。
- view-strict.txt：-std=c11 -O2 -Wall -Wextra -Werror，真实SDK头文件下最终view编译exit0，成功日志无输出。

原课程build_firmware入口、SDK2.5.1/sf32lb52-lchspi-ulp整机编译成功（sdk-first.txt），既有SDK告警保留，无新roll3告警。main.bin **1,244,176字节**，SHA256 **aefb41ff3731fc25f60191a69d7ab99a17eb81a1ebb0ae1129beb1fee32fc56d**。firmware.json含源码/三镜像/工程保护项/字体/补丁指纹；补丁正向/逆向检查通过，旧版全部已归档源码/三镜像/保护项复核保持。未修改受保护脚本或工程配置。

45条严格需求追踪通过；原new_devlog已刷新，课程总检查仍有8条历史提交格式和14天历史提交空档失败、manifest四个身份/提交/会话字段待填，未改写历史/编造数值。183项源码/镜像/工程/补丁指纹一致，diff检查通过，见trace-final.txt、course-checks.txt与final-audit.json。新真机风格/触摸/动画/连续运动和流畅度均待证据，不将上版照片记成本版通过。hw和feat提交正文已备，按课程git-workflow须负责人实际读过确认后才能提交，当前未提交/推送或覆盖既有改动。

## 烧录和验收

关闭串口助手，连接USB-to-UART；下载程序询问COM号时只输入数字。

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy RemoteSigned -Force
$env:Path = "$env:USERPROFILE\.local\bin;$env:Path"
. 'D:\SiFli-SDK\v2.5.1\export.ps1'
Set-Location 'D:\course_starter\build\m3-pixel-20261112'
& '.\project\build_sf32lb52-lchspi-ulp_hcpu\uart_download.bat'
```

串口1000000，入口应为`build=m3-pixel-20261112`。请以同位置照片确认视野/计时/菜单/中文风格，用视频覆盖校准/准备、慢滚与坡边、暂停→继续/回点、首次踩绿继续运动及0.8秒结算；若仍卡顿附帧诊断。证据存本目录board-run01。新源/字库/工程需完整重编，不能混旧对象。

这一步需要你在真实硬件上验证并回传证据（日志/照片/截图，存进 evidence/），我需要看到证据才能继续。

复现主机：用既有Zig Python执行tests/run_maze_host.py --zig和tests/run_roll3_sdk_view.py。字体生成/校验为原tools/generate_maze_font.py加--sdk D:/SiFli-SDK/v2.5.1 --roll3 --pixel（校验另加--check），固定Pillow12.3.0/fontTools4.68.0。
