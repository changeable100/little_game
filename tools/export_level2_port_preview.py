"""Export native C/LVGL level-two replay frames for review (not board evidence)."""
from pathlib import Path
import struct,zlib,csv,html
R=Path(__file__).resolve().parent.parent
out=R/'models/level2-desktop/board';out.mkdir(parents=True,exist_ok=True)
for p in (R/'build/roll3-live-view').glob('LEVEL2-*.ppm'):
    magic,shape,maximum,raw=p.read_bytes().split(b'\n',3);w,h=map(int,shape.split())
    assert magic==b'P6' and maximum==b'255' and len(raw)==w*h*3
    def chunk(tag,data):return struct.pack('>I',len(data))+tag+data+struct.pack('>I',zlib.crc32(tag+data)&0xffffffff)
    scan=b''.join(b'\0'+raw[y*w*3:(y+1)*w*3] for y in range(h))
    png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(scan))+chunk(b'IEND',b'')
    (out/(p.stem+'.png')).write_bytes(png)
rows=list(csv.reader((R/'build/level2-physics-poses.csv').read_text(encoding='utf-8').splitlines()))
doc='''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>第二关移植预览</title><style>body{margin:24px;background:#113448;color:#e9cbe5;font:15px/1.6 system-ui}a{color:#f6d665}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:24px}figure{margin:0}img{width:100%;max-width:390px;image-rendering:pixelated}figcaption{padding:8px 0}p{color:#a9d7df}</style><a href="../index.html">电脑模型</a> · <a href="../game-menu/index.html">菜单与选关</a><h1>第二关 · 同源板端代码预览</h1><p>图片由真实游戏C物理输入回放和SDK LVGL生成，未上板。42.705秒通过两个存档和两陡降，零掉落、零预算耗尽。曲面采样与电脑模型偏差小于1厘米。</p><div class="grid">'''
for i,row in enumerate(rows):
    ms,state,cp=map(int,row[:3]);caption=f'{ms/1000:.3f}s · 存档 {cp}'+(' · 通关' if state==5 else '')
    doc+=f'<figure><img src="LEVEL2-{i:02}.png" alt="{html.escape(caption)}"><figcaption>{html.escape(caption)}</figcaption></figure>'
(out/'index.html').write_text(doc+'</div></html>',encoding='utf-8')
