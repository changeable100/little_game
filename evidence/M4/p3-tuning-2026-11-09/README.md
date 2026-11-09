# P3首轮：减速、低倾角响应和镜头平滑

2026-11-09。基于62d1418及未提交的近景版继续，用户反馈惯性/灵敏度偏高和运动抖动。不是新板测PASS。当前新镜像指纹在本目录firmware.json，旧close-view指纹为上一版，不能用来校验本轮。

## 改动和软件验证
- 输入死区0.045g、满幅0.45g、渐进曲线，小倾角加速下降，大倾角仍可输出4.9m/s²用于爬坡。
- 赛道滚动阻力0.005→0.08，滑动摩擦/反弹系数不变；不改自由落体或物理时钟。
- 镜头XY120ms/Z200ms平滑、微小位移死区，状态切换/重开立即定位；平台水平顶面接缝消除重叠。
- 116项核心主机回归PASS；包含平地1m/s回平2.5s后速度<0.08m/s、位移<1.5m，能量不增加。新旧坡道/两次转弯连续通关通过。
- 真实SDK LVGL跟随抖动死区/单调收敛/复活新局/无效快照/七态按钮/20轮资源释放PASS；SDK2.5.1编译PASS，原RWX/newlib/entry告警保留。108000字节视图缓冲不变，新增少量相机状态，无新增timer。
- 真实帧率未知；67ms为UI定时配置，不能据此声称实机15FPS。若仍跳动需视频区分镜头/格纹/刷新问题。

## 烧录前准备与镜像核验
确认设备管理器COM号，关闭占用串口的监视器，在PowerShell执行：

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy RemoteSigned -Force
$env:Path = "$env:USERPROFILE\.local\bin;$env:Path"
. 'D:\SiFli-SDK\v2.5.1\export.ps1'
Set-Location 'D:\course_starter\build\maze-integration'
$record = Get-Content 'evidence/M4/p3-tuning-2026-11-09/firmware.json' -Raw | ConvertFrom-Json                                                                                                               
foreach ($image in $record.images) {
    $actual = (Get-FileHash -LiteralPath $image.path -Algorithm SHA256).Hash
    if ($actual -ne $image.sha256) { throw "镜像指纹不符 $($image.path)" }
    Write-Host "OK $($image.path)"
}
.\project\build_sf32lb52-lchspi-ulp_hcpu\uart_download.bat
```

按提示输入COM数字。烧录后打开1000000 baud、8N1、无流控串口并开始保存日志，再复位。

## 同一次短视频完成对照
1. 启动应有 `[roll3:selftest] summary PASS failures=0` 和 `[roll3:boot] ready=1`，进入三维挑战、平放校准。
2. 在起点轻倾、回平，观察微调是否容易及滑行是否缩短，不需要达到主机测试的精确1m/s。
3. 沿原坡道、检查点、两个转弯游玩，拍摄移动过程中地图；区分整图上下晃、格纹闪烁、固定节奏跳动。用较大倾角确认仍能爬坡。
4. 暂停/继续，掉落回检查点、返回再进，检查镜头是否立即回到正确位置。

把完整serial.txt、短视频或关键照片放本目录board-run01（大视频可附链接），反馈“灵敏度/滑行/抖动”三个感受。失败保留原日志。需要看到真实证据才能判断手感/抖动是否通过，合成图片只供布局核对。
