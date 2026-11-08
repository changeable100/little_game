# 平放重测后 TIMING ERROR（2026-11-08）

来源：学生先反馈“放平之后timing error”，随后应诊断请求直接提供一组四行内容，原消息为单行，逐字保存于user_status_original.txt。没有提供失败前连续日志、照片、精确操作时刻或板端固件指纹；按本轮6a3a05d上下文关联，未独立校验烧录版本。保留run01的INPUT失败，不覆盖。

实际字段：run=2、rev=6、state=5、error=3、elapsed=0、cal=32；bias_mg=-20,-33；pos_milli=1500,1500。确认32样本校准完成，此后触发TIMING错误，尚未累计RUNNING用时。不是本次校准失败；不将校准子项成功扩大为生产游戏可玩性通过。age=160408为错误后旧发布的年龄，不能当成故障发生时的推进间隔，也不能由此算出具体卡顿时长。

源码判据：maze_controller_tick在RUNNING时检测now-anchor_ms>MAZE_MAX_DT_MS（100ms），在消费新样本前优先按时序失败处理；PAUSE结算同样调用该入口。不截短dt、不更改100ms或校准阈值。

静态排查：maze_runtime.c的20ms LVGL timer在revision变化时调用生产renderer；maze_ui.c每次渲染重设256格及起终点底色、重复设置标签，并隐藏再显示标记/球。SDK v2.5.1 external/lvgl_v9/src/core/lv_obj_style.c中lv_obj_set_local_style_prop无同值提前返回，调用lv_obj_refresh_style；后者invalidate对象。lv_label_set_text_vfmt同样invalidate、释放旧动态文本并重新格式化。这是已证实的多余刷新路径，可能造成延迟，但当前没有板端耗时记录，不能宣布已定位唯一根因。显示flush、其他LVGL回调和线程调度耗时尚待区分。

下一步拟做局部刷新并补低频/故障时耗时记录，主机保留既有时序失败断言，编译后由学生重新板测；涉及UI/runtime/测试，先提交计划给学生确认。本轮无源码或阈值修改，尚未提交/推送。
