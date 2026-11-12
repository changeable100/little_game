# 三维英文界面（REQ-527 / TC-527）

负责人提供像素中文错误页实拍，明确要求使用英文。原图/指纹/限定结论在../roll3-pixel-2026-11-12/board-run01。照片显示输入异常与00:00，不能确定采样、校准或中断原因；本次没有改物理/传感器保护，也不把改语言写成输入故障已修复。

三维页面的校准、准备、菜单、暂停/回存档点、掉落/错误、结算和退出统一简短英文，保持粉紫像素风及既有交互。MENU用16px ASCII，说明20px、标题28px；字库只需95个可打印ASCII及星形，共96字符×3字号，仍1bpp/2×2块。完整源码/工程在本分支根目录，build标记m3-english-20261112；首发11提交已推送56c95716，英文为后续修订。

## 验证与失败保留

- view-before.txt：新增宽度检查最初错误引用SDK不存在的lv_text_get_width，编译FAIL。查固定SDK实际lv_text_get_size后重测，旧中文在ASCII/星形判据FAIL，保留view-before-retest.txt。
- view-complete.txt：真实SDK LVGL七态/20轮、ASCII/字形覆盖/可见文字宽度与MENU命中、全部真实按钮/暂停确认/旧事件/重校准/800ms结算/下一关禁用/销毁重入回绕PASS。新增绘图大缓冲申请0次，不外推板端全堆或FPS。
- font-generation.txt/font-check.txt：96字符×16/20/28完整覆盖，版本锁与OFL不改。view-strict-final.txt：-Wall/-Wextra/-Werror最终view编译exit0，成功无输出。
- sdk-final.txt及sdk-complete.txt：原python scripts/build_firmware.py，SDK2.5.1/sf32lb52-lchspi-ulp编译PASS。最终main.bin1,222,392字节，SHA256 8ae4cceb48d6a7508c3a14f012e7d266f000a3698b974aef44b43361980d03bd；三镜像、改动源码和补丁指纹在firmware.json。
- 物理/会话/渲染器生产代码与首发版本语义逐项一致，保留本次已重跑162项核心PASS；本轮只重跑与文案/字库有关的SDK UI/严格编译/构建，不重复声称新增核心项。
- trace-before-report.txt保留新报告尚未补入时的追踪FAIL；补实际结果后trace-complete.txt为46条PASS。published-pixel-push.txt保留首发pre-push警告日志缺“明日计划”，已恢复小节，course-complete.txt显示当日日志无问题；旧8条历史提交/14天空档FAIL和清单4缺项WARN保留。

ENGLISH-gallery-final.png及ENGLISH-menu-error-result.png为实际SDK主机出图，已查看，非真机或概念图。最终错误文案采用泛指INPUT ERROR，避免在无串口时猜测INPUT LOST。初图和早期日志保持，最终以complete/final与firmware.json为准。

## 烧录与验收

关闭串口助手，使用USB-to-UART；下载提示COM号时输入数字。

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy RemoteSigned -Force
$env:Path = "$env:USERPROFILE\.local\bin;$env:Path"
. 'D:\SiFli-SDK\v2.5.1\export.ps1'
Set-Location 'D:\course_starter\build\roll3-publish-20261112'
& '.\project\build_sf32lb52-lchspi-ulp_hcpu\uart_download.bat'
```

串口1000000。验收英文是否可读/完整，校准→READY、MENU→RESUME/CHECKPOINT、绿色接触后继续运动和800ms结算，NEXT LEVEL仍禁用。若仍出现INPUT ERROR，回传完整启动和错误附近的串口诊断；照片不推断故障原因。证据保存本目录board-run01。

这一步需要你在真实硬件上验证并回传证据（日志/照片/截图，存进 evidence/），我需要看到证据才能继续。
