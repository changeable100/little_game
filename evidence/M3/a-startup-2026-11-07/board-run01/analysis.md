# A启动保障板端正常路径复核（2026-11-07收到）

startup_original.txt与home_original.jpg均为原附件逐字节复制，哈希/大小见metadata.json。照片原件保留，没有裁剪/增强。准确拍摄/运行时刻及独立板端提交/固件哈希未提供；按学生对602eaf7烧录说明的回复上下文关联，不能从SDK横幅build on Oct31推定实际执行日，也不能用旧11月6日app固定文案独立锁定版本。

- 模型24项、地图4项、适配器25项共53项板上自检及三组summary均PASS/failures0。这些是板上逻辑自检，不是69项主机/注入测试全部在板上执行。
- runtime=1/result0在menu ready之前，符合REQ-408/TC-408正常初始化顺序，运行时和菜单创建成功。
- 首页照片可见MOTION/PUZZLE、TILT MAZE、PREVIEW、共享地图缩略图、关卡信息及ENTER MAZE/HOW TO PLAY两个完整入口，TC-201首页显示子项PASS。本张照片不证明按钮点击、六态实际切换或倾斜屏幕随动。
- 保留ft6146的两个E级日志id_H=64/id_L=56。核对本机SDK customer/peripherals/touch_panel/ft6146/ft6146.c第308～317行：仅在read_regs返回RT_EOK时用LOG_E打印ID；这两行本身不是读失败证据。不据此改SDK或要求重烧，触摸实际点击仍须后续证据。

结论：本轮正常启动及首页可见性通过，解除新正常启动取证等待。队列/timer分配失败和菜单失败回收仍只有主机注入覆盖，未在真机制造故障；未新增本轮IMU校准/四向、触摸导航、通关或生产错误提示通过结论。软件构建时firmware.json中的physical_verified=false保留其当时含义，当前物理结果追加于此和manifest，不修改历史记录。
