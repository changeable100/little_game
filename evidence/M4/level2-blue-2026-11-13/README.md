# 第二关冰蓝配色与专用试玩烧录包

2026-11-13，负责人要求地图换色、绿色终点不变，并索取烧录命令。冰蓝路面0x66b9d6、第二弯0x528ed1、青蓝槽/护栏0x2b879e、深蓝背景0x17364d、橙色箭头0xffb55f；终点0x8ac879、终点护栏0x4a9f66及浅棋盘0xffece5全部保持。palette-validation.txt确认去掉RGB后生成C逐字一致，几何、摩擦、索引/掩码及交互未变。

SDK主机11姿势/暂停/结算及原资源/英文回归通过，见sdk-view.txt与gallery.png；实际查看蓝色画面及绿色终点。edge-final.txt：129867/129869候选在原量化容差内，保持99%阈值，100/50行及哨兵一致。生产入口仍M3；为方便本次上板，tools/build_roll3_m4_preview.py复制src/project到build/m4-board-preview，副本仅runtime/page的world选择M4，并标记build=m4-blue-preview-20261113。旧demo和启动自检保持M3及M4全覆盖，工程骨架原样复制、课程构建脚本未修改。preview-runtime-final.txt用真实runtime和M4终点位置验证生命周期/IMU释放/自动复活/结算，并显式断言模型world是M4。

sdk-preview-build.txt为首次全量SDK编译，sdk-preview-final.txt为专用标记的最终增量编译，均通过，原SDK链接告警保留。main.elf中已核对专用标记；镜像大小/SHA256及源指纹见firmware.json。未烧录、无新真机通过。

## 烧录（PowerShell）

先关闭串口助手：

```powershell
. 'D:\sifli\SiFli-SDK\v2.5.1\export.ps1'
& 'D:\peripheral_lab\build\m4-board-preview\project\build_sf32lb52-lchspi-ulp_hcpu\uart_download.bat'
```

当前注册表识别COM8，脚本提示serial port num时输入8。若连接变化，以设备管理器显示为准。使用SDK生成的完整脚本，已核对bootloader@0x12010000、main@0x12020000、ftab@0x12000000，不只刷main.bin。烧录后进入“3D挑战”即第二关；正式NEXT LEVEL仍未接入。
打开串口日志使用1000000波特率，进入后应有[roll3:open] build=m4-blue-preview-20261113。请回传启动/校准/两弯/两次陡降/绿色终点与结算的日志或录像后再记录真机验收。

需要重建时，SDK导出后在D:/peripheral_lab运行python tools/build_roll3_m4_preview.py。该工具只构建，不烧录。

## 跨盘路径修复
用户从C:调用时SDK原脚本cd未切盘，造成镜像路径失败，串口/stub成功不代表刷写完成。已改cd /d和WORK_PATH绝对镜像参数，tools/build_roll3_m4_preview.py重建后会自动修正生成脚本。替身从C:执行验证三路径/地址/SHA通过，未访问设备、固件未改。原错误和修复日志均在本目录。
