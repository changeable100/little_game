# A第三阶段：启动保障与B接入

2026-11-07；基线398d725。负责人已确认仅做A侧main/runtime启动保障和边界测试，B继续负责按钮、页面和生产适配。

REQ-408：UI拥有线程在菜单创建前完成runtime初始化。提供只读`maze_runtime_init_status()`，返回NOT_INITIALIZED=0、READY=1、SELFTEST_FAILED=2、LEVEL_FAILED=3、QUEUE_FAILED=4、TIMER_FAILED=5。只在主/LVGL线程查询（包括初始化之前）；不能从其他线程轮询这个非原子状态。查询不推进、分配资源或启动采样；READY只代表运行时资源已建立，不代表已经校准/可RESUME。契约v1及既有动作/快照API保持。

失败后既有action/attach/snapshot/next_request/detach仍返回WRONG_STATE，不给出部分初始化的快照。timer创建失败须detach已经初始化的队列；队列失败不得detach不存在的队列。允许初始化失败后主线程显式重试，成功后重复init返回WRONG_STATE，但只读状态保持READY，不覆盖成功状态或创建第二个timer。

main顺序：三组现有自检→lab_init/显示初始化→runtime_init及启动结果日志→菜单创建→LVGL循环。lab/显示失败直接返回，不创建runtime或菜单；runtime失败仍可展示B现有模拟菜单，但启动状态为失败。B生产入口必须先查询READY，否则禁用游戏入口并用对应原因提示；这部分页面实现归B，不把现有模拟预览当可运行游戏。本轮不创建第二个UI时钟，不修改B菜单或契约，不添加I2C操作。

菜单创建失败而runtime已就绪时，main调用`maze_runtime_cancel_startup()`回收timer/队列再退出。该入口仅UI线程、未进入首局(run_id=0)、无render绑定/采样时有效；运行后或attach后返回WRONG_STATE，不作为游戏退出API。成功回收后状态NOT_INITIALIZED，可重新init。TC-408另覆盖菜单失败回收和误用拒绝；LVGL删除接口以本SDK的lv_timer_delete声明为准。

TC-408（关联REQ-408、REQ-209、REQ-406）：覆盖冷启动查询、所有未就绪API拒绝、自检失败无资源、队列失败无timer且不错误detach、timer失败队列回收及后续成功、成功后重复init不覆盖READY、不新增timer。主入口测试调用生产main.c，替身记录调用顺序，覆盖lab/显示失败，三组自检分别失败，runtime失败仍先于menu且状态可见，菜单失败返回，以及正常循环。RT/LVGL服务替身不等于板测，不在真机故意制造资源故障。

主机回归通过后SDK编译，真机需新启动日志确认`[maze:boot] runtime=1 result=0`先于菜单创建成功日志，再验证首页正常；失败路径由主机注入，不要求学生在一秒内输入或破坏资源。此前run07/08仅为旧固件证据，不能当本轮新启动已通过。

本轮软件结果：69项主机回归和SDK编译通过，main.bin1089256字节。新烧录在已激活SDK的终端进入`D:\course_starter\build\maze-integration`，关闭串口监视器后运行`.\project\build_sf32lb52-lchspi-ulp_hcpu\uart_download.bat`，再重连1000000 baud/CRLF并复位。保存完整启动原文（原模型24/地图4/适配器25项自检及上述boot两行）和首页照片至新的证据目录；本轮UI仍为模拟预览，生产入口禁用/错误提示由B下一批实现。全部启动就绪不代表采样/校准已启动。
