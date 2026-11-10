# TC-512 游玩提速和轻倾响应
旧版17.24秒视频及用户反馈见[视频记录](../roll3-input-2026-11-10/board-run01/analysis.md)。画面可见持续运行及球位变化，用户报告严重卡顿和轻倾滞后后突进。无串口，未量出旧板端帧时与端到端延迟。

新版本游玩时执行中心采样，静态保留局部四点；球网格用固定三角值，标签仅在内容变动时更新。边缘在游玩时可能比暂停时更明显。降低小角度死区和提高首样本跟随，中等倾角加速更缓，保留满幅及采样保护。

主机同36帧渲染器平均1.436ms（旧运行路径局部四点）降至0.465ms（游玩中心采样），约减少68%，不含LCD传输与RT调度。单采样分带与整帧深度逐像素相同。TC-512输入曲线、原回归和LVGL图像质量选择见同目录日志。

## PowerShell 烧录
```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy RemoteSigned -Force
$env:Path = "$env:USERPROFILE\.local\bin;$env:Path"
. 'D:\SiFli-SDK\v2.5.1\export.ps1'
Set-Location 'D:\course_starter\build\maze-integration'
& '.\project\build_sf32lb52-lchspi-ulp_hcpu\uart_download.bat'
```
先关闭串口监视器，提示输入串口时填COM后的数字。新镜像指纹为本目录firmware.json。

## 一次复测
串口1000000 baud、8N1，开始记录后复位。进3D挑战并平放校准，分别试轻倾起步、稍加倾角、回平。录一段可看到手和画面的短视频及数行 `[roll3:frame]`，放本目录board-run01。用 draw_max_ms 和 gap_max_ms 区分绘制与帧间隔，样本年龄只描述数据新鲜度。真机手感和端到端改善待复测。
