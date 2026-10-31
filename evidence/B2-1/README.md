# B2-0 / B2-1 验证记录（2026-10-31）

本目录均为主机/编译记录。PNG 是 SDK LVGL 软件渲染，输入明确为模拟数据；没有进行固件烧录、屏幕/触摸物理测试或 IMU 测试。

## 可复现命令

在仓库根目录，使用可启动的 Python 及 MinGW GCC（本机 GCC 4.9.2）：

```powershell
$env:CC = 'D:\DevC++\Dev-Cpp\MinGW64\bin\gcc.exe'
$env:PATH = 'D:\DevC++\Dev-Cpp\MinGW64\bin;' + $env:PATH
python tests/render.py --sdk D:\sifli\SiFli-SDK\v2.5.1 --out build\maze-preview --maze-only
python tests/render.py --sdk D:\sifli\SiFli-SDK\v2.5.1 --out build\maze-preview --ui-only
```

本机成功主机运行用 bundled Python 3.12.14；SDK venv Python 3.13.15 也已核实可启动，但在沙箱中受访问限制。宿主可执行环境重跑通过。需要 SDK 的 `external/lvgl_v9`，不使用伪 LVGL。`--maze-only` driver 为 `tests/maze_ui_test.c`；`--ui-only` 为原 Lab UI driver 加入口/返回断言。构建缓存与 PPM 位于忽略目录 build；PNG 仅由 PPM 无损格式转换。

固件在 SDK v2.5.1 / GCC 14.2.1 环境下：

```powershell
cd project
python -m SCons --board=sf32lb52-lchspi-ulp -j8
```

本机手工设置 `SIFLI_SDK`、`SIFLI_SDK_PATH` 为 `D:/sifli/SiFli-SDK/v2.5.1`，`RTT_CC=gcc`、`RTT_EXEC_PATH=D:/sifli/tools/tools/arm-none-eabi-gcc/14.2.1/bin`；`PYTHONPATH` 包含 SDK `tools/build` 和 `tools/build/default`；PATH 包含上述 GCC bin 及 SDK Python Scripts。未改项目配置。

## 按次保留结果

| 文件 | 真实结果 |
|---|---|
| host_2026-10-31.txt | 首次启动SDK Python失败，未执行测试 |
| host_retry_2026-10-31.txt | 首次实际主机测试失败：READY文本断言不符 |
| host_diagnostic_2026-10-31.txt | 沙箱运行测试超时，不能推断功能结论 |
| host_status_diagnostic_2026-10-31.txt | 宿主诊断：实际状态文本为 `...`，保留失败 |
| host_fix_retry_2026-10-31.txt | 修复LVGL省略号模式后主机测试通过，判据未放宽 |
| lab_ui_integration_2026-10-31.txt | 真实Lab入口/返回断言及原有UI回归通过 |
| build_2026-10-31.txt | 首次固件构建通过（文字修复前） |
| build_final_2026-10-31.txt | 最终源码固件构建通过，main.bin 1,547,836字节 |
| course_checks_2026-10-31.txt | new_devlog/check_trace/check_all/build_firmware均因本库无scripts目录无法执行 |
| review_2026-10-31.txt | 本次统一需求/TC手工追踪检查、A五文件与受保护文件差异检查 |
| maze-preview.png / maze-entry.png | 独立driver / Lab入口的软件截图，二者为相同固定模拟输入 |
| dashboard.png | 加入入口后的Lab首页软件截图 |

完整构建日志仅移除行尾空白并在首行标注；其余日志仅移除末尾空白行，诊断内容及失败记录保留。最终增量编译仍报告 RWX LOAD segment 和 `img "dfu" not found`；首次链接也保留已有 newlib/entry 等输出，未屏蔽或改骨架。

## 统一用例结论

- TC-201：主机候选布局PASS；物理显示/触摸未执行。球是8×8对象，地图320×320。
- TC-202/210：模拟页无生产sink且游戏按钮禁用，BACK退出；生产动作及六态策略未执行。
- TC-203：固定READY/0s及等待不自行累计PASS；动态六态/暂停/通关冻结未执行。
- TC-205/209：20轮创建/渲染/BACK/重复销毁，2000次重复render，顶层对象数与timer数回到基准PASS；不等同真机堆无泄漏或生产时序通过。
- TC-206：NULL/NaN/越界/ID不匹配/非法地图元数据/非READY输入安全隐藏球PASS；旧局/完整动态异常未执行。
- TC-207/208：保留失败与重测、明确来源/命令/限制；没有新增物理验证结论。
- TC-204/211/212/213：本次未重跑地图及连续模型用例；原 B@943e762 失败/重测日志未变。

源基线和编号映射见 `docs/maze-stage2-handoff.md`。本次修改的提交号用 `git log --oneline origin/feat/maze-integration..HEAD` 获取；不把自己提交号预写成未发生的事实。
