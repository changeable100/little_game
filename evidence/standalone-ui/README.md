# 独立游戏界面验证（2026-10-31）

用户明确要求去除Peripheral Lab测试界面，确认独立游戏首页、迷宫与玩法说明。默认入口已由Lab改为游戏菜单；Lab源文件只作为历史排障/测试资源保留，生产固件不再调用其UI、外设worker、BLE/PAN启动接口。SDK底层自动初始化的驱动/组件未通过改配置裁剪，不能宣称全部蓝牙/音频代码都已移除。

## 源码与界面

- `src/main.c`：保留模型24项和地图4项启动自检、littlevgl2rtt显示/触摸初始化和现有LVGL主循环，改为maze_menu_create。
- `src/maze_menu.c/.h`：新建独立首页和说明，导航到现有静态迷宫，BACK回自己的首页；菜单销毁清理自身及迷宫覆盖页。
- `src/maze_ui.h`：更新预览入口导航说明；不改A五个文件、地图数据或工程配置。
- `game-home.png` / `game-help.png` / `game-maze.png`：真实SDK LVGL软件渲染，已目视检查，无Lab卡片，沿用真实板图反馈后的安全边距。
- 地图小图由maze_levels_default直接绘制，没有复制地图；仅LVGL图元和现有Montserrat字体，无新增图片/字体许可依赖或像素缓冲。

## 可重跑主机验证

在仓库根目录设置可用Python及MinGW GCC，SDK版本v2.5.1：

```powershell
$env:CC = 'D:\DevC++\Dev-Cpp\MinGW64\bin\gcc.exe'
$env:PATH = 'D:\DevC++\Dev-Cpp\MinGW64\bin;' + $env:PATH
python tests/render.py --sdk D:\sifli\SiFli-SDK\v2.5.1 --out build\maze-standalone --maze-only
```

本机Python3.12.14、MinGW GCC4.9.2；`tests/maze_ui_test.c`用生产菜单/迷宫代码和SDK LVGL，不用伪造LVGL。完整输出在host_2026-10-31.txt。链接沿用GCC响应文件以支持Windows长目录。

| 用例 | 实际结果与边界 |
|---|---|
| TC-201 | 独立首页/说明/迷宫主机截图已检查，390×450；新版物理布局未执行 |
| TC-202/210 | ENTER MAZE仅导航，不提交START；操作仍禁用且PREVIEW显著，说明提示运动控制尚未启用 |
| TC-205/209 | 20轮首页→说明→返回→迷宫→返回、带打开页面销毁、重复创建/销毁后对象与timer数量回基准；无新增游戏timer |
| TC-203/206 | 原固定READY/0s、不自行计时、非法静态输入和2000次重复render仍PASS；六态/旧局动态异常尚未实现 |
| TC-208 | 本目录、ADR-0002和docs/maze-standalone-ui.md提供版本/命令/限制；固件SHA256见firmware_integrity.json |

没有将按钮事件回调模拟称为真实触摸或物理资源泄漏验收。旧Lab UI测试源码/证据保留为旧路径，不是新产品入口。

## 固件与静态核查

激活原SiFli SDK v2.5.1 / GCC14.2.1环境后在project目录执行：

```powershell
python -m SCons --board=sf32lb52-lchspi-ulp -j8
```

本机环境设置同evidence/B2-1/README.md，不修改proj.conf或构建骨架。build_2026-10-31.txt为完整输出，仅移除行尾/末尾空白并标注；编译成功，main.bin为905,548字节。仍保留SDK的RWX段、newlib syscall stub、entry及缺少DFU镜像警告，未消音或删功能凑通过。

link_review_2026-10-31.txt记录最终ELF符号核对：maze_menu_create及两组自检保留，lab_ui_create/lab_init/lab_ble_init/lab_pan_init未出现在已定义符号中。原型改动不修改A模型/契约五文件与构建配置。

课程scripts/new_devlog.py、check_all.py、build_firmware.py仍不存在；实际尝试错误见course_checks_2026-10-31.txt。不报告课程总检查通过，不把A模板工程的检查结果挪成本库结论。

## 待学生新版本物理验证

此前真实照片只证明旧Lab入口版的静态迷宫，不替代新入口验收。请烧录当前生成的完整uart_download.bat配置，再复位：保存包含两组自检及`[maze:app] standalone game menu ready; preview controls only`的完整启动日志；拍摄游戏首页/说明/迷宫；分别验证说明与迷宫BACK均回游戏首页、不出现Peripheral Lab。

新版本真实显示/触摸、IMU、生产动作、动态六态、整机通关和物理堆测试均未验收。本次只交付已确认的独立界面改版，不伪造可玩的生产输入。
