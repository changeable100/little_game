"""Convert exact SDK LVGL PPM frames to PNG and the offline review gallery."""
from pathlib import Path
import struct,zlib
R=Path(__file__).resolve().parent.parent
out=R/'models/level2-desktop/game-menu';out.mkdir(parents=True,exist_ok=True)
for p in (R/'build/game-menu-view').glob('GAME-*.ppm'):
    magic,shape,maximum,raw=p.read_bytes().split(b'\n',3);w,h=map(int,shape.split())
    assert magic==b'P6' and maximum==b'255' and len(raw)==w*h*3
    def chunk(tag,data):return struct.pack('>I',len(data))+tag+data+struct.pack('>I',zlib.crc32(tag+data)&0xffffffff)
    scan=b''.join(b'\0'+raw[y*w*3:(y+1)*w*3] for y in range(h))
    png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(scan))+chunk(b'IEND',b'')
    (out/(p.stem+'.png')).write_bytes(png)
(out/'index.html').write_text((R/'tools/game_menu_preview.html').read_text(encoding='utf-8'),encoding='utf-8')
