"""Run native SDK LVGL menu or gameplay fixtures with installed Zig."""
from pathlib import Path
import os,runpy,subprocess,sys,ziglang
root=Path(__file__).resolve().parent.parent
zig=Path(ziglang.__file__).parent/'zig.exe'
original=subprocess.run
def run(command,*args,**kwargs):
 if isinstance(command,(list,tuple)) and command and command[0]==str(zig):command=[str(zig),'cc',*command[1:]]
 return original(command,*args,**kwargs)
subprocess.run=run
os.environ['CC']=str(zig);os.environ['CFLAGS']=''
view='--roll3-view' in sys.argv
if view:os.environ['ROLL3_LEVEL2_POSES']=str(root/'build/level2-physics-poses.csv')
sys.argv=[str(root/'tests/render.py'),'--sdk','D:/sifli/SiFli-SDK/v2.5.1','--out',str(root/'build'/('roll3-live-view' if view else 'game-menu-view')),'--roll3-view' if view else '--game-menu']
try:
 runpy.run_path(str(root/'tests/render.py'),run_name='__main__')
except SystemExit as error:
 if error.code not in (0,None):raise
if not view:runpy.run_path(str(root/'tools/export_game_menu_preview.py'),run_name='__main__')
