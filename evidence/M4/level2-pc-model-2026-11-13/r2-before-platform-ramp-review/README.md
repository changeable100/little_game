# 第二关电脑模型 r2

双击 index.html 离线打开，也可访问 http://127.0.0.1:8767/ 。左键旋转、右键平移、滚轮缩放；“游戏跟随视角”与右侧“沿线位置”组合检查球附近结构。所有台柱默认显示。

本版按负责人新参考重新排布：两弯内半径为0.33/0.25m，转轴相差90°；尖脊和曲面塔柱围成紧凑回转。整关9.97×9.82m，默认斜视从左上到右下展开。绿色终点已移到高层前方，没有压在上层平台下面。两个存档均直接平移第一关20面棕边黄心圆盘，尺寸和颜色不变，取消自创金色SAVE台与旗杆。

遮挡检查首次发现53条视线被第一孔横梁/第二孔前墙挡住；第二次剩5条。已保留两次失败，实际改为第一孔朝镜头敞口、第二孔前边随坡降低、后墙保留。最终354个沿线位置、1416条球体视线无遮挡，19块承载路面之间平面面积重叠为0。检查使用默认跟随方向 yaw=-2.4、pitch=0.8、距离9m；任意旋转角度不承诺全部可见。球沿线示意并非物理解算。

44个对象（36结构/8标记）、6048个非退化三角形。level2-desktop.glb 为米制Y-up glTF2，level2-desktop.obj 为米制Z-up带RGB顶点模型。model.json 为可读导出数据。无外部纹理、网络或查看器依赖。

生成源：tools/m4_desktop_layout.py；网格与导出：tools/build_m4_desktop_model.py；查看器：tools/m4_desktop_viewer.html。source-seed.json仅是旧板端参考快照，r2不再以它生成布局。

```powershell
cd D:\peripheral_lab
python tools/build_m4_desktop_model.py
python tools/check_m4_desktop_model.py
```

生成只用Python标准库；校验需要本机已有NumPy。validation.txt / visibility-checks.json 为独立读取导出网格的校验结果。截图为实际电脑浏览器画面：whole-view、top-view、checkpoint-view、follow-view、goal-view。

继续只做电脑模型。未移植此布局、未新编译/烧录固件、未验收物理通关。旧169项核心测试和37.15秒回放不适用于本版模型。r1被否决的原文件及两次r2遮挡失败保存在 evidence/M4/level2-pc-model-2026-11-13。
