# 成员B第三阶段首批：P0/P1（2026-11-07）

基线 `cfa87d01a4f5c7e560ed3314469a06c5847ef1fb`，远端 `origin/codex/maze-ab-merge`，包含e076e48。功能分支 `codex/maze-ui-stage3-render`。用户明确确认先交P0/P1；本记录是B对真实接口的核对与下一批建议，不冒称A已签署生产接入评审。

## P0接口核对

已读取maze_contract.h v1、maze_runtime.h/.c、main.c和集成计划。实际runtime已经提供next_request、action、snapshot、attach/detach；旧头文件中“请求号适配缺失”的说法不再作为当前事实。现有main先创建菜单、再runtime_init，LVGL事件在后续handler循环执行；初始化失败时的生产入口禁用/提示仍须P2联评，不能仅假定timer存在。

P2建议沿用UI三函数，新增适配层负责：创建成功后attach一次；按显示快照的run_id与next_request生成动作；无论返回值均重读snapshot；render回调只读，不递归action/detach。BACK成功后detach再destroy，临时关闭只detach保留局面。UI错误码与maze_result_t分离。未明确生产清理入口前，本批拒绝非NULL sink，首页仍进入显著标注的模拟预览，不attach、不初始化第二个runtime、不创建时钟。

## 实现前REQ/设计/TC判据

| REQ | 本批设计 | TC与实现前预期 |
|---|---|---|
| REQ-201/203 | 同一地图和只读快照驱动球位、状态、elapsed_ms | TC-201/203：六种合成快照生成六张真实LVGL截图；20ms显示0s；暂停/通关同快照在主机等待后不自行加时；最大UINT32毫秒可格式化 |
| REQ-202/210 | 仅渲染状态文案，PAUSED第二按钮显示RESUME；本批所有游戏动作禁用，BACK仅退出模拟页 | TC-202/210：六态均不提交动作、不伪造校准；生产矩阵和恢复返回值留P2联评 |
| REQ-205/209 | 对象只在create分配，重复render不增加对象/timer；destroy幂等，新实例不被旧revision抑制 | TC-205/209：保留20轮与2000次render；六态轮换、重复revision、新实例低revision均能显示；生产detach流程未实现 |
| REQ-206 | 地图结构/ID、有限半径坐标及边界、状态/input/error枚举分别校验 | TC-206：NULL/NaN/Inf/越界/非法格值/维度/端点/未知枚举安全提示并隐藏球；输入/计时/关卡ERROR有明确原因，关卡无效时不绘球，不撤销数据检查 |
| REQ-207/208 | 主机模拟、生产代码回归、编译与物理证据分层 | TC-207/208：输出标模拟；58项既有回归、SDK构建与指纹，保留失败；无新硬件结果不改physical_verification |

保留390×450、地图(35,61)/320×320、20px格和已板测圆角边距；球以快照圆心乘20再减像素半径，当前r=0.20格。状态提示位于y=40、地图前的空隙，PREVIEW标记不移除。UI不做碰撞/积分/第二份计时，不直接读取controller/传感器。

本批不改main、runtime、controller、motion service、契约/模型或工程配置。集成基线恢复的内部lab_io是生产采样依赖，不因产品不显示Lab而删除。课程检查用可用原脚本或路径适配包装器，缺失/既有问题如实记录，不改受保护检查。


## 成员B P0/P1交付记录（2026-11-07追加）

用户确认先交P0/P1，基于远端已发布cfa87d01a4f5c7e560ed3314469a06c5847ef1fb建立codex/maze-ui-stage3-render。接口核对表和REQ/TC先落字，随后扩展六态、错误原因、枚举/地图/球坐标校验、快照时间和PAUSED的RESUME文案；PREVIEW和全部游戏动作禁用保留。未修改main/runtime/controller/motion/契约模型/菜单或工程配置，不恢复/删除内部lab_io。

真实SDK LVGL主机六态及异常输入测试PASS；20轮导航/销毁及2000次六态轮换render后对象/timer回基准；20ms显示0s、UINT32时间、临时快照值不被保留、等待不自行加时、新实例低revision可显示均通过。六张PNG与总览已实际查看，无空白图。旧58项生产逻辑/runtime边界回归PASS。SDK v2.5.1/GCC14.2.1编译PASS，main.bin1089824字节，三镜像及源码SHA256见evidence/M3/ui-render-2026-11-07/firmware.json，RWX/newlib/entry/DFU警告保留。未取得新的板端UI/触摸/IMU/通关证据，不改physical_verification。

本库无课程scripts/config，使用build内忽略的参数化路径桥接器执行本机可用的未改动课程原脚本，未将本机绝对路径发布成通用工具。首次桥接器误假设所有main接受argv，开发日志检查抛TypeError；同时bundled Python缺PyYAML。保留初次输出，修正桥接器调用签名并改用已有PyYAML的SDK解释器，最终需求追踪PASS；历史8项署名判定/14天历史提交间隔FAIL及manifest学号/组别/正式提交时间/会话导出路径4项缺失WARN保留，不伪造或改保护脚本消警告。本轮功能测试无失败，补强2000次六态轮换后另留最终主机结果，不覆盖初次通过记录。

学生实际确认P0/P1范围，无人工代码修正/否决；B对runtime接口完成核对，但没有冒称A已审阅签署。按提交正文审阅要求准备两份可读草稿，待学生读过确认后小步提交。本批下一步由A审查六态/坐标/指针与边界，再推进P2生产适配，不要求把当前模拟截图当板端状态通过。
