# TC-514：A 侧 H1 手感独立复测
基于 GitHub 集成分支 `1ab64e22b9066e1d8d95086a2855b19399398827`，共同代码基线 `d422c3c`。A 分支 `codex/roll3-downhill-a` 在旧地图上验证输入。B 从共同基线建立路线分支。

H1在0.15g和0.30g稳态分别约0.81、2.39m/s²，斜向合力限到4.9m/s²。接近平平台、接地且回平时按速度轻刹；不更改斜坡重力、离地和反倾。旧代码新增用例两项FAIL（core-baseline），新125项核心通过（core-first）。初速1m/s，平地0.8秒速度≤0.30m/s、滑行≤0.55m的仿真判据通过。真机手感仍待证据。

## 烧录与一次对照
先关串口监视器。使用同目录firmware.json核对新镜像。PowerShell：
```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy RemoteSigned -Force
$env:Path = "$env:USERPROFILE\.local\bin;$env:Path"
. 'D:\SiFli-SDK\v2.5.1\export.ps1'
Set-Location 'D:\course_starter\build\maze-integration'
& '.\project\build_sf32lb52-lchspi-ulp_hcpu\uart_download.bat'
```
提示时填COM后的数字。烧录后串口1000000 baud、8N1，复位并保留三项TC-514自检。进3D校准，在起点平平台做轻倾→加大倾角→回平，再做反倾；短视频尽量拍到手和球。坡上检查回平仍有自然滚动，不把平平台制动判据套到坡上。发视频和几行[roll3:frame]，材料进本目录board-run01。
