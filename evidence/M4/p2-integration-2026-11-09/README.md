# 2026-11-09 三维生产接入

B 的 787ab37、1052681 以快进方式纳入当前集成分支。审查发现 2 倍放大把图像延伸到说明区，缩放调整为约 1.63 倍。投影自检现在只有在中心投影成功后比较方向坐标。

| 验证 | 结果 |
|---|---|
| 核心主机 | 114 项通过，原始日志为 `evidence/M2/maze_adapter/host_2026-11-09_133321.txt` |
| 三维 LVGL | 透视、遮挡、七态、布局、正式按钮、返回与对象/缓冲释放通过，页面使用运行时替身 |
| 二维 LVGL | 原生产页面、迟帧暂停和返回回归通过，主机使用 RT 与传感器替身 |
| 字库 | 三维 201 字形与二维 273 字形校验通过 |
| SDK 2.5.1 | 编译通过，三段镜像见 `firmware.json` |

首次 SDK 构建报自检缺少 `stddef.h`，首次核心主机编译报断言关闭导致变量未使用。保留 `sdk-first.txt` 和 `core-first.txt`，修正后的输出另存。SDK 原有 RWX、newlib 和入口符号警告保留。

## 1. 准备 PowerShell 和板卡

本轮目标是本地提交 `62d1418` 生成的三维整机固件，板型为 `sf32lb52-lchspi-ulp`。电脑上的 SDK 目录是 `D:\SiFli-SDK\v2.5.1`，实际工作目录是 `D:\course_starter\build\maze-integration`。

1. 用能传数据的 USB 线接板子的 **USB-to-UART** 接口。打开 Windows **设备管理器 → 端口（COM 和 LPT）**。插拔一次，对比新增的 USB 串口并记下 COM 号。之前用过 COM3，以这次显示为准。
2. 断开 VS Code SiFli 串口监视器，关闭其他正在占用该端口的串口工具。烧录完成后再重新连接。
3. 新开一个 PowerShell，执行下面这段。在这个窗口里继续后面的镜像检查和烧录。

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy RemoteSigned -Force
$env:Path = "$env:USERPROFILE\.local\bin;$env:Path"
. 'D:\SiFli-SDK\v2.5.1\export.ps1'
Get-Command sftool -ErrorAction Stop
Set-Location 'D:\course_starter\build\maze-integration'
```

导入时应看到 `Activating SiFli-SDK v2.5.1`，随后 `Get-Command sftool` 应返回工具所在路径。导入命令最前面的点后有一个空格，它让 SDK 工具在当前 PowerShell 生效。执行策略只作用于这一个窗口。

## 2. 核对镜像并烧录

现成镜像已编译。先在同一窗口核对三份镜像，全部显示 `OK` 即可直接烧录。

```powershell
$record = Get-Content -LiteralPath 'evidence/M4/p2-integration-2026-11-09/firmware.json' -Raw |
    ConvertFrom-Json
foreach ($image in $record.images) {
    if (-not (Test-Path -LiteralPath $image.path -PathType Leaf)) {
        throw "找不到镜像 $($image.path)"
    }
    $actual = (Get-FileHash -LiteralPath $image.path -Algorithm SHA256).Hash
    if ($actual -ne $image.sha256) {
        throw "镜像指纹不符 $($image.path)"
    }
    Write-Host "OK $($image.path)"
}
```

如果镜像缺失，可在 SDK 已激活的窗口从当前 P2 工作副本重新构建。先用 `git log -1 --oneline` 核对固件源码版本，构建成功后记录新镜像的 SHA-256。重新构建的文件用其新指纹关联本次板测。

```powershell
Set-Location 'D:\course_starter\build\maze-integration\project'
scons --board=sf32lb52-lchspi-ulp -j8
if ($LASTEXITCODE -ne 0) { throw '编译失败，请保留构建输出' }
Set-Location 'D:\course_starter\build\maze-integration'
Get-FileHash -Algorithm SHA256 -LiteralPath @(
    'project/build_sf32lb52-lchspi-ulp_hcpu/main.bin',
    'project/build_sf32lb52-lchspi-ulp_hcpu/bootloader/bootloader.bin',
    'project/build_sf32lb52-lchspi-ulp_hcpu/ftab/ftab.bin'
)
```

在集成工作目录执行 SDK 生成的脚本。脚本一次写入启动程序、应用和分区表。

```powershell
.\project\build_sf32lb52-lchspi-ulp_hcpu\uart_download.bat
```

出现 `please input the serial port num:` 时只输入数字。例如设备管理器显示 `COM3`，就输入 `3` 并回车。等工具完成写入后进入下一步。如果工具提示端口无法打开，先确认实际 COM 号和监视器是否已经断开。

## 3. 开始记录串口，再复位

重新打开串口监视器，选同一个 COM 口，设为 **1000000 baud、8N1、无流控**。如果用串口发送命令，发送行尾设为 **CRLF**。先开始记录，再按板上的复位键。如果串口已经可用，也可发送 `reboot`。

启动后在日志中找下面三行，屏幕应到达首页并显示 **3D挑战**。

```text
[roll3:selftest] summary PASS failures=0
[roll3:boot] ready=1
[maze:boot] menu ready
```

## 4. 在一局内完成三维验证

板子保持屏幕朝上、文字方向正对自己。把线留出余量，避免 USB 线把板子拉斜。用屏幕按钮完成操作，串口只用于记录。

| 步骤 | 怎么做 | 通过时看到什么 |
|---|---|---|
| 进入与校准 | 点 **3D挑战**。把板放平，点 **开始**，手离开，等页面切换。 | 显示 **请放平校准**，随后显示 **游玩中**。日志依次出现 `state=1` 和 `state=2 cal=32`。 |
| 右、上方向 | 轻轻压低屏幕右沿，让右沿更靠近桌面，看到球开始移动后回平。再压低屏幕上沿，看到移动后回平。若球已贴在对应边缘，可先轻倾反方向留出空间。 | 右沿压低时球往屏幕右方移动，上沿压低时球往画面上方或远处移动。 |
| 暂停与继续 | 点 **暂停**，轻倾板子并等几秒，再回平，点 **继续**。 | **已暂停** 时球位和用时冻结。继续后回到 **游玩中**，原局继续计时。串口出现 `state=3` 再回 `state=2`。 |
| 掉落与复活 | 轻倾到平台边缘，让球滚出，等到 **小球掉落**。放平板子后点 **回检查点**。 | 日志出现 `state=4`。按按钮后恢复 **游玩中**，掉落数保留，用时接着累计。页面显示检查点 0 时回起点，显示检查点 1 时回已到达的检查点。 |
| 返回与再进 | 点 **返回**，等首页出现后再进入一次 **3D挑战**。 | 回到首页。串口记录 `state=0` 及本次 IMU 的 `stopped result=0 settings_saved=1`。再次进入显示 **准备开始**。 |

本轮验收点到返回和再进为止，通关留作后续手感测试。日志中的 `[roll3] run=` 是游戏局次，`[maze:imu] run=` 是共用 IMU 服务的会话号。对照同一个 IMU 会话的启动和停止行即可。

## 5. 保存这一次的材料

将完整串口日志和一段能看清移动、复活的短视频放进本轮的 `board-run01/`。方便拍照时，可用游玩中、已暂停、复活后的照片替代视频，再附一句右、上两轴是否同向。文件夹可用下面的命令创建。

```powershell
New-Item -ItemType Directory -Force -Path 'D:\course_starter\build\maze-integration\evidence\M4\p2-integration-2026-11-09\board-run01'
```

串口日志可命名为 `serial.txt`。若某一步失败，保留发生时的画面与前后日志，再另起 `board-run02/` 重试。遇到乱码先核对 1000000 baud；找不到 **3D挑战** 时核对启动日志和镜像指纹；出现 **输入异常** 时留存 `[maze:imu]` 与 `[roll3]` 行，再平放点 **重开** 试一次。
