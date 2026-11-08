# P1真三维物理内核与B渲染接入

负责人王若豪（A）已确认低多边形真3D、碰撞手感优先及独立物理任务方案A。本批在`codex/maze-3d-physics`实现P1物理内核，开发起点`f276000`，负责人已读hw/feat正文并确认本地保存；尚未发布新的远程提交号。P0新板证已齐，不再让学生重测二维页面。本批没有三维生产页面/独立任务，不能说现在板上已能玩3D。

## A已提供的生产接口

| 文件 | 职责 |
|---|---|
| src/roll3_contract.h | 独立的三维版本、SI单位、状态/动作/快照、静态OBB几何与材质 |
| src/roll3_physics.h/.c | 单所有者模型；无RT/LVGL/I2C、无动态分配；固定5ms推进、连续扫掠/冲量/摩擦/姿态 |
| src/roll3_scene.h/.c | 同一套静态短赛道：起点、真坡道、上平台、窄桥、终点、侧墙与检查点 |
| src/roll3_selftest.h/.c | 27组与主机同源的板上启动自检；实验失败不阻断原二维入口 |

`ROLL3_CONTRACT_VERSION=1`与旧`MAZE_CONTRACT_VERSION=2`相互独立，不能强转、memcpy或依靠相同枚举数字复用。世界为右手坐标：x右、y前、z上；米、秒、kg，球半径0.20m、质量1kg，均匀实心球I=2mr²/5。`r3_box_t`包含center/half/三个正交单位axis，材质restitution/friction/rolling_friction与RGB888 color；渲染和碰撞必须共用这些数据，不能另画一张看似相同的二维地图。

核心入口：`r3_world_validate`检查世界、`r3_init`建立READY，`r3_action`处理START/PAUSE/RESUME/RESTART/BACK/RESPAWN，`r3_calibrated`完成当前run校准，`r3_step`只收5ms，`r3_snapshot`复制完整快照。返回OK0/INVALID-1/WRONG_STATE-2/STALE-3/EXHAUSTED-4。请求非零且严格递增，expected_run必须匹配；新START/RESTART/BACK使run递增，计时/检查点/掉落计数清零。输入非法转INPUT ERROR，重开重新校准；P1合成用例直接完成校准，不代表生产绕过32稳定样本。

快照含position/velocity/omega、世界角速度对应的orientation四元数(w,x,y,z)、radius、state/error、run/revision、elapsed_ms、falls/checkpoint、contacts/contact_normal/grounded，以及limited/limited_steps和speed_limited。四元数用于球面花纹随滚动转动，不能只移动圆点。elapsed只累计完成的RUNNING 5ms步，暂停不计；PAUSE清线速度和角速度。FALLEN后手动RESPAWN回当前检查点（未激活则起点），保持run/用时/掉落数；到达goal后WON冻结。

接触采用球对OBB最近点和面/边/角连续扫掠，法线冲量含恢复系数，摩擦按接触点v+ω×r及球转动惯量计算，滚阻耗能。速度8m/s、角速度80rad/s上限触发speed_limited；单步最多8次碰撞/修正，预算耗尽limited并停止本步运动，limited_steps饱和累计。不是无限精度物理引擎；真正手感须P2后学生体验调参，参数不可为凑自检通过随意改。

## B下一步：三维视图和中文流程

1. 先只做UI线程的create/render/destroy视图，输入为完整`r3_snapshot_t`和只读`r3_world_t`；用合成快照做主机预览。三维物理实例由A未来独立任务独占，B不调用r3_step、不新建游戏推进timer、不读I2C，也不调用旧maze_runtime去控制三维球。
2. 固定透视相机，画真实OBB及低多边形球。使用一个小视口/缓冲输出到LVGL，不给每个三角形建对象；从约160×180内部视口和少量三角形试起，依据实际成本调整，不能预先承诺FPS。RGB565+16bit深度在该视口约115200字节，分配失败应有中文提示/返回，不在每帧分配。
3. 必须有深度遮挡、近裁面/视口边界检查，球到墙后会被遮挡；球的投影半径随透视深度变化。给球加简单双色面或条纹，使用四元数体现滚动。坡道直接取OBB axis，窄桥允许从边缘掉下去，不在画面另加隐形护栏。
4. 页面中文覆盖校准放平、游玩、暂停/继续、掉落/回检查点、通关/重开/返回。READY不假装开始，CALIBRATING只有真实校准完成才变RUNNING；按钮只发操作意图，等待A的生产适配器返回并重读。先做视图，不伪造与现有二维生产入口已经连接。
5. B交付时明确固定相机位置/目标、屏幕左右/前后对应世界方向。A统一把已验证IMU屏幕轴映射到相机水平基；B不能再次反向/交换输入，避免重演二维轴向反复实测。相机改变必须通知A同步映射。

建议先交一批纯视图和主机截图，再交生命周期/中文控件；A同步做P2独立物理任务、有界队列、完整快照和采样会话清理。保留二维版本作为可用入口，三维入口待A/B整合后再启用。没有生产API时把按钮禁用并标明主机预览，不能发旧动作凑演示。

## B的交付验证与边界

记录真实几何/快照版本、固定相机、内部分辨率、buffer数量/尺寸、分配失败/销毁与重复进入测试。主机覆盖透视缩放、球与墙深度遮挡、坡道顶面/侧面、无非法写像素、四元数旋转、全部状态/中文和控件边界，20轮打开关闭资源回基线。三维游戏页仍不能冒称物理触摸/帧率/手感验收。

不改A的roll3_physics/selftest、旧模型、工程骨架、课程检查器/配置，不删既有失败记录。遇到共享契约缺字段先给出具体使用场景，由A统一补；不要私建一个重复物理模型或把旧二维单位当米。先读docs/01～03，补TC关联REQ-501后实现；按课程格式写提交正文供负责人审阅。

## 本轮验证与下一次板测

109项核心主机PASS（原81项+三维27项+启动隔离1项），SDK编译PASS；原始失败/修正与指纹见`evidence/M4/p1-physics-2026-11-08`。本轮不要求为新自检重复烧录旧UI。待P2三维画面和独立任务整合，一次列清最少操作/日志/照片，再验证新27项板上自检、真实倾斜/掉落复活和实际帧时/操控手感。
