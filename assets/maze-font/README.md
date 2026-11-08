# 迷宫中文静态字库

源字体为所用SiFli SDK v2.5.1中`external/lvgl_v9/scripts/built_in_font/SourceHanSansSC-Normal.otf`（思源黑体Normal）。字体内name表声明Adobe版权与SIL Open Font License 1.1，生成前已核对。完整许可文本见OFL.txt，源文件SHA256、版权/许可原文、字符清单与工具版本见provenance.json。仓库仅保存重命名为maze_font_16/20/28的生成子集及许可，不保存整份源字体。

使用已有SDK源字体生成16/20/28px、不压缩4bpp、无kerning的LVGL9字库；中文字形与标点来自这三个产品UI文件，额外包含可打印ASCII用于数字与格式化错误码。生成器先用字体cmap验证每个字符真实存在，再栅格化；主机LVGL测试还检查界面标签没有英文字母/缺字占位，实际PNG需人工查看。

```powershell
uv run --no-project --with pillow==12.3.0 --with fonttools==4.68.0 python tools/generate_maze_font.py --sdk D:\SiFli-SDK\v2.5.1
uv run --no-project --with pillow==12.3.0 --with fonttools==4.68.0 python tools/generate_maze_font.py --sdk D:\SiFli-SDK\v2.5.1 --check
```

更新中文文案后重新生成并check，检查生成C字库差异后再主机/SDK编译。仅运行项目固件构建无需安装Python字体工具；既有src/SConscript自动收集C文件，未修改proj.conf或启用动态字库服务。字符/位图静态数据位于只读段；新增字库镜像大小与真实RAM/显示性能分别评估，不用源码C文件大小冒充镜像增量。
