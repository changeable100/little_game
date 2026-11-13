"""Generate static LVGL 9 4bpp UI subsets from SDK Source Han Sans (SIL OFL).

uv run --no-project --with pillow==12.3.0 --with fonttools==4.68.0 python
  tools/generate_maze_font.py --sdk D:/SiFli-SDK/v2.5.1 [--check]
Uses source literals plus printable ASCII. The legacy script name is retained for font provenance. It now generates
only the current 3D product fonts; --roll3 remains a compatibility option.
"""
import argparse,hashlib,json,re
from importlib.metadata import version
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
from fontTools.ttLib import TTFont

root=Path(__file__).resolve().parent.parent
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--sdk',required=True,type=Path)
parser.add_argument('--check',action='store_true')
parser.add_argument('--roll3',action='store_true',default=True,help='Generate separate B 3D-view fonts; existing maze subsets unchanged')
parser.add_argument('--pixel',action='store_true',help='Roll3 only: monochrome half-size glyphs enlarged 2x; original 5x7 digits')
args=parser.parse_args()
assert not args.pixel or args.roll3, '--pixel requires --roll3'
assert version('pillow')=='12.3.0' and version('fonttools')=='4.68.0', 'Use the pinned Pillow/fontTools versions in this script usage'
source=args.sdk/'external/lvgl_v9/scripts/built_in_font/SourceHanSansSC-Normal.otf'
font=TTFont(source)
license_text=next(n.toUnicode() for n in font['name'].names if n.nameID==13)
assert 'SIL Open Font License' in license_text
chars=set(chr(c) for c in range(32,127))
for name in ('roll3_view.c','roll3_menu.c'):
    for literal in re.findall(r'"(?:[^"\\]|\\.)*"',(root/'src'/name).read_text(encoding='utf-8')):
        chars.update(c for c in json.loads(literal) if ord(c)>=32)
chars=sorted(chars,key=ord)
missing=[c for c in chars if ord(c) not in font.getBestCmap()]
assert not missing,('source font missing characters',missing)

def write(path,content):
    if args.check:
        assert path.read_text(encoding='utf-8')==content, f'stale font: {path}'
    else:
        path.parent.mkdir(parents=True,exist_ok=True)
        path.write_text(content,encoding='utf-8',newline='\n')

sizes=(16,20,28)
asset_dir='roll3-font'
digits={
 '0':['01110','11011','11011','11011','11011','11011','01110'],
 '1':['00100','01100','00100','00100','00100','00100','01110'],
 '2':['01110','11011','00011','00110','01100','11000','11111'],
 '3':['11110','00011','00011','01110','00011','00011','11110'],
 '4':['00011','00111','01111','11011','11111','00011','00011'],
 '5':['11111','11000','11000','11110','00011','11011','01110'],
 '6':['01110','11000','11000','11110','11011','11011','01110'],
 '7':['11111','00011','00110','00110','01100','01100','01100'],
 '8':['01110','11011','11011','01110','11011','11011','01110'],
 '9':['01110','11011','11011','01111','00011','00011','01110'],
}
for size in sizes:
    logical_size=size//2 if args.pixel else size
    face=ImageFont.truetype(str(source),logical_size)
    boxes=[face.getbbox(c,anchor='ls') for c in chars]
    if args.pixel:
        digit_h=max(7,round(logical_size*.72));digit_w=round(digit_h*5/7)
        boxes=[(0,-digit_h,digit_w,0) if c in digits else b for c,b in zip(chars,boxes)]
    ascent=max(-b[1] for b in boxes);descent=max(b[3] for b in boxes)
    if args.pixel:ascent*=2;descent*=2
    bitmap=[];glyphs=['    {0}, /* reserved */']
    for c,(left,top,right,bottom) in zip(chars,boxes):
        width=right-left;height=bottom-top
        start=len(bitmap)
        if width and height:
            image=Image.new('L',(width,height))
            if args.pixel and c in digits:
                image=Image.new('L',(5,7));image.putdata([255*int(v) for row in digits[c] for v in row])
                image=image.resize((width,height),Image.Resampling.NEAREST)
            else:ImageDraw.Draw(image).text((-left,-top),c,font=face,fill=255,anchor='ls')
            if args.pixel:
                image=image.point(lambda p:255 if p>=96 else 0).resize((width*2,height*2),Image.Resampling.NEAREST)
                pixels=[int(v!=0) for v in image.get_flattened_data()]
                bitmap.extend(sum((pixels[i+j] if i+j<len(pixels) else 0)<<(7-j) for j in range(8))
                              for i in range(0,len(pixels),8))
            else:
                pixels=[(v*15+127)//255 for v in image.get_flattened_data()]
                bitmap.extend((pixels[i]<<4)|(pixels[i+1] if i+1<len(pixels) else 0)
                              for i in range(0,len(pixels),2))
        advance=(digit_w+1)*32 if args.pixel and c in digits else round(face.getlength(c))*32 if args.pixel else round(face.getlength(c)*16)
        if args.pixel:left*=2;bottom*=2;width*=2;height*=2
        assert len(bitmap)<(1<<20) and advance<4096 and max(width,height)<256
        glyphs.append(f'    {{.bitmap_index={start}, .adv_w={advance}, .box_w={width}, .box_h={height}, .ofs_x={left}, .ofs_y={-bottom}}}, /* U+{ord(c):04X} */')
    bytes_c='\n'.join('    '+','.join(f'0x{n:02x}' for n in bitmap[i:i+20])+',' for i in range(0,len(bitmap),20))
    offsets=','.join(str(ord(c)-32) for c in chars)
    symbol=f'roll3_font_{size}'
    code=f'''/* Generated by tools/generate_maze_font.py. Source Han Sans SC Normal.
 * SIL Open Font License 1.1; see assets/{asset_dir}/OFL.txt and provenance.json.
 * Uncompressed {1 if args.pixel else 4}bpp, no kerning; {'2x block pixels / original 5x7 digits' if args.pixel else 'source coverage checked'}. */
#include "lvgl.h"
static const uint8_t bitmap[]={{
{bytes_c}
}};
static const lv_font_fmt_txt_glyph_dsc_t glyphs[]={{
{chr(10).join(glyphs)}
}};
static const uint16_t unicodes[]={{{offsets}}};
static const lv_font_fmt_txt_cmap_t cmaps[]={{{{
    .range_start=32,.range_length={ord(chars[-1])-31},.glyph_id_start=1,
    .unicode_list=unicodes,.glyph_id_ofs_list=NULL,.list_length={len(chars)},
    .type=LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
}}}};
static const lv_font_fmt_txt_dsc_t descriptor={{
    .glyph_bitmap=bitmap,.glyph_dsc=glyphs,.cmaps=cmaps,.kern_dsc=NULL,
    .kern_scale=0,.cmap_num=1,.bpp={1 if args.pixel else 4},.kern_classes=0,.bitmap_format=0
}};
const lv_font_t {symbol}={{
    .get_glyph_dsc=lv_font_get_glyph_dsc_fmt_txt,
    .get_glyph_bitmap=lv_font_get_bitmap_fmt_txt,
    .line_height={ascent+descent},.base_line={descent},.subpx=LV_FONT_SUBPX_NONE,
    .underline_position=-2,.underline_thickness=1,.dsc=&descriptor
}};
'''
    write(root/'src'/f'{symbol}.c',code)
metadata={'source':'SDK v2.5.1 external/lvgl_v9/scripts/built_in_font/SourceHanSansSC-Normal.otf',
          'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
          'copyright':next(n.toUnicode() for n in font['name'].names if n.nameID==0),
          'license':license_text,'license_url':'http://scripts.sil.org/OFL',
          'generator':f'Pillow {version("pillow")} + fontTools {version("fonttools")}; uncompressed {1 if args.pixel else 4}bpp; no kerning',
          'pixel_grid':2 if args.pixel else 1,'monochrome_threshold':96 if args.pixel else None,
          'digit_design':'Original 5x7 matrices in generator' if args.pixel else 'Source Han Sans SC',
          'sizes':list(sizes),'characters':''.join(chars),'character_count':len(chars)}
write(root/'assets'/asset_dir/'provenance.json',json.dumps(metadata,ensure_ascii=False,indent=2)+'\n')
write(root/'assets'/asset_dir/'OFL.txt',(args.sdk/'external/lvgl_v9/tests/src/test_files/fonts/noto/OFL.txt').read_text(encoding='utf-8'))
print(f'Font {"check" if args.check else "generation"} PASS: {len(chars)} glyphs x {len(sizes)} sizes, complete source coverage')
