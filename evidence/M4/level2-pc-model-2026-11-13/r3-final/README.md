# 第二关电脑模型 r3

双击 index.html 离线打开，或访问 http://127.0.0.1:8767/ 。左键旋转、右键平移、滚轮缩放。

本次修订对应负责人两张截图：
- 上方陡降入口平台从1.6m加宽到2.6m，长2.4m；下方中继从2.0m加宽到2.8m，长1.8m。实体台柱随平台加宽。
- 第二处反向弯改成回转前短直坡（4.65→4.20m）、等高半圆（4.20m）、回转后短直坡（4.20→3.75m）。半圆没有纵向落差或径向扭曲。
- 为容纳短坡，第一高层模块向后平移0.4m，第二存档及短桥相应调整。第一关同款存档圆盘、冰蓝路面、绿色终点、实体台柱和对角线推进保留。

顶部“短坡半圆”和“加宽平台”直接查看这次两处修改。“游戏跟随视角”配合右侧沿线位置滑块巡看球附近结构。台柱默认显示。截图 whole-view.png、bend-view.png、platform-view.png 均为此版本实际浏览器画面。

独立读回检查：范围10.77×10.22m，20块承载面之间平面面积交叠0；379个沿线位置地面连续，1516条球体视线无阻（固定跟随yaw=-2.4/pitch=.8/距离9m）。两短坡为平面、半圆所有上表面顶点z=4.20m，坡/弯两侧接缝准确吻合。47对象（39结构/8标记）、6162非退化三角形，GLB2范围通过。检查记录见validation.txt和visibility-checks.json。任意旋转角度/全部可玩偏移位置未穷举；沿线示意不作物理仿真。

level2-desktop.glb 为米制Y-up glTF2；level2-desktop.obj 为米制Z-up带RGB顶点模型；model.json 为导出数据。查看器没有外部纹理或网络依赖。

```powershell
cd D:\peripheral_lab
python tools/build_m4_desktop_model.py
python tools/check_m4_desktop_model.py
```

生成源为tools/m4_desktop_layout.py，导出工具为tools/build_m4_desktop_model.py，查看器模板为tools/m4_desktop_viewer.html。生成使用Python标准库，独立校验需已有NumPy。source-seed.json仅为旧板端参考快照，r3不使用该快照生成布局。此前版本及失败记录保存在evidence/M4/level2-pc-model-2026-11-13。

本轮只更新电脑模型，尚未移植、未新构建/烧录固件、未作物理通关验收。
