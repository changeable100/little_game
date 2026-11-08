# B第三阶段P2：适配接口与实施计划（待A/负责人核对）

2026-11-07。依据用户提供的P2任务书；开发基线为已发布的`d4520a74a531dccd5afef7eafa486bc0aa40d38f`，包含B的P1和A启动接口602eaf7。当前文档是开发前方案，不表示接口已经实现或A已签署。

## 本次目标与边界

在`codex/maze-ui-stage3-runtime`新分支完成P2软件交付，分两批：启动门控/真实打开/快照/临时释放；完整按钮/控件矩阵/所有销毁路径/连接回归。完成后准备提交正文供负责人审阅；功能分支发布按当次授权执行，不替A合并。

修改maze_ui.c/.h、maze_menu.c/.h，新增maze_ui_runtime.c/.h与测试；不改main、runtime、controller、motion service、Lab worker、契约/模型/地图、启动自检和SDK配置。保留69项既有回归与P1纯UI测试。真实动态画面、触摸/通关及资源复用待A合并后的新板端证据。

## 拟新增公开入口（签名待核对）

```c
/* UI错误码0/-100段；仅main/LVGL线程，不重复初始化runtime。 */
int maze_ui_runtime_open(void);

/* 临时关闭：只解除自己持有的绑定再释放UI，幂等；
 * 返回runtime detach结果，绝不发送BACK或重置模型。 */
maze_result_t maze_ui_runtime_release(void);

/* 查询最近打开/动作/读取/关闭的runtime结果，供导航错误提示。 */
maze_result_t maze_ui_runtime_last_result(void);

/* 模拟测试保留独立入口，生产main仍调用maze_menu_create。 */
int maze_menu_create_preview(void);
```

maze_ui_create(sink, context)继续作为纯视图入口：NULL sink表示模拟，非NULL sink表示生产适配。视图复制显示快照，持有完整按钮命中区域与状态矩阵；事件携带显示局号交给适配sink，request_id由适配调用runtime_next_request分配。视图不创建模型、计时、采样或分配请求号。

拟增加视图生命周期通知接口：`void maze_ui_set_delete_handler(void (*handler)(void *), void *context)`，只能在本次UI创建后、绑定runtime前设置。页面DELETE事件在清除本模块缓存/释放对象之前调用handler；适配根据自身的bound标记解除引用，禁止递归destroy。正常release先清bound、detach，再destroy，因此删除通知不会重复detach。此接口是防护外部LVGL删除的最后路径，不代替正常先detach后destroy顺序。

生产BACK由适配sink执行：动作、重读、成功后release；事件函数在调用sink后立即返回，不再访问可能已销毁的页面。非生产BACK仅关闭模拟页。页面事件携带生命周期标识，拒绝旧页面延迟事件；标识只用于UI所有权，不作为request_id分配器。

## 打开与清理调用关系

1. 首页create及ENTER事件查询init_status，仅MAZE_RUNTIME_READY允许生产进入。其他状态禁用ENTER并显示初始化/自检/关卡/队列/timer的明确原因；HOW TO PLAY仍可用。启动READY不表示已校准。
2. open拒绝重复打开；创建自身上下文和生产视图，注册删除通知，再attach一次。attach失败只清理本次视图，不调用detach，不清除别人的render绑定。
3. attach成功标记bound，立即snapshot。首次快照成功前全部游戏操作不可用；读取失败才对自己已绑定的renderer做detach并清理，保留错误反馈。后续render_wrapper只复制快照值并渲染；不缓存借用指针、不递归action/detach。
4. 临时release、maze_menu_destroy、home_deleted及视图外部删除均走同一持有绑定清理逻辑，不调用cancel_startup，不隐式BACK/RESTART。detach失败的处理须结合A接口语义：现有实现会在owner线程先清renderer，再进行暂停/停采样；该结果仍需显示/记录，不冒称正常暂停。
5. runtime的应用级20ms timer保留；关闭后后台停止采样按现有异步流程完成，B不抢工作槽。快速重进按实际PAUSED/其他快照显示。

## 动作与显示规则（REQ/TC实施前判据）

REQ-202/210、TC-202/210：START仅READY；PAUSE仅RUNNING，PAUSED改RESUME；RESTART在RUNNING/PAUSED/WON及可恢复INPUT/TIMING ERROR启用；READY/CALIBRATING禁用重复重开，LEVEL/未知错误禁用重开。BACK可用于已有快照页面。所有控制还受有效快照与耗尽锁定约束。

每次意图以显示快照run_id调用next_request，再action(NULL, &action)，失败也snapshot重读。STALE丢弃、不自动改局重发；WRONG_STATE按实际状态显示；INVALID有明确反馈；EXHAUSTED停止产生新意图，提示重启，允许临时释放路径解除引用。START后以真实快照显示CALIBRATING或ERROR，不伪造RUNNING。生产文案更新，模拟页保留标记。

REQ-201/203/206、TC-201/203/206：保留P1六态、非法地图/坐标/枚举校验、安全边距与快照时间。动作失败提示不得被同步重读立刻吞掉；无效快照禁用危险动作并显示原因。

REQ-205/209、TC-205/209：覆盖重复打开/关闭、外部删除、带页面菜单销毁、快速BACK重进、旧页面事件、旧run、attach失败不detach他人、attach成功后snapshot失败回滚、耗尽和恢复样本未就绪。保留20轮/2000次render与对象/timer基准。

REQ-408、TC-408：六种启动状态与未知值有安全门控；生产入口就绪与模型READY分开。模拟菜单显著标注并独立测试。

## 两层验证及交付

第一层使用SDK真实LVGL运行纯UI六态/异常测试和PNG；第二层链接生产适配代码、生产runtime.c、controller/model与真实LVGL，RT/传感器传输可用替身，避免与tests/runtime_stubs/lvgl.h混用类型。对难以通过真实runtime触发的错误可在测试链接边界做有标识的故障注入，必须同时保留正常路径真实连接；不能以完整假runtime代替生产连接结论。

完成69项A回归、SDK构建、三镜像SHA256及PNG实际查看，记录原始失败/重测、基线、变更文件与REQ/TC。课程检查使用未改原脚本；历史问题如实保留。新增纯逻辑如需板上自检接入A拥有main时，先列需求给A，不自行改main或伪称已接。

## 待确认

请A/负责人核对上述公开签名、视图删除通知、BACK成功才关闭/临时释放保留局面、状态矩阵及真实runtime连接测试方案。确认后先更新docs/01、02、03并实施；如需调整A核心接口，列出具体场景和期望返回交A处理。


## B第三阶段P2生产接入（经A/负责人确认方案后追加）

用户明确回复A/负责人已核对接口方案并确认开发。基于d4520a74a531dccd5afef7eafa486bc0aa40d38f建立codex/maze-ui-stage3-runtime，先更新REQ/设计/TC。实现maze_ui_runtime_open/release/last_result和startup_text，视图删除通知、代际事件防护、生产与明确模拟菜单分开。生产路径查询启动状态，attach后立即snapshot，不隐式START；五动作从显示run领runtime请求号，任何返回均重读；BACK仅成功才detach/destroy，临时释放保留局面，菜单和外部删除提前解除回调。未改A核心、main、Lab采样依赖或SDK配置。生产菜单/说明/页面已移除模拟文案，纯UI测试仍显著模拟。

纯UI六态/异常、20轮/2000render PASS；连接生产runtime.c/controller/model及SDK真实LVGL的测试PASS，覆盖实际校准/倾斜模型移动及球像素位置、五动作、暂停失败转TIMING ERROR后重读和重开、启动失败门控、拒绝绑定不detach他人、读取失败回滚、旧run/旧页面事件、全部结果/耗尽、20轮退出重进和所有销毁路径。RT/传感器替身与链接故障注入明确标记；WON/INPUT ERROR控件图为合成矩阵，不冒称完整生产通关。PNG实际查看，最后调整首页启动信息到右侧列以避免覆盖小地图。

新增测试初次缺rtthread.h导致类型不完整，补齐后通过；初次ARM编译发现适配文件缺stddef.h，主机旧GCC隐式间接包含未暴露此问题，按首处错误修复而未删功能，最终编译PASS。A回归使用旧GCC4.9时主入口setjmp测试进程0xC0000028异常退出，原始日志保留；离线Zig缓存不存在，按A命令经现有本机代理在uv临时环境获取ziglang0.13后，69项原断言通过。未修改A测试/断言或全局配置。SDK最终main.bin1092656字节，三镜像指纹和完整输出已存evidence/M3/ui-runtime-2026-11-07，SDK旧RWX/newlib/entry/DFU警告保留。

课程原脚本通过已有build路径桥接运行：追踪PASS；历史8项署名判定、历史14天断档FAIL及manifest四项缺失WARN仍在，保护脚本未改。原日志按指定2026-11-07开发日刷新，A既有记录保留，本节只追加B工作；实际运行自动日志时间保留，不改提交时间。学生实际介入为确认A接口方案，无人工代码修正/否决。提交正文另供负责人审阅；未新烧录，physical_verification未增添，软件连接不等于触摸/IMU/WON/物理资源复用通过。
