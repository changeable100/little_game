"""Build an isolated M4 board preview with the original course/SDK build scripts.

Run after exporting the SDK environment. No flashing or production entry changes.
"""
import argparse
import importlib.util
import json
from pathlib import Path
import shutil
import sys

ROOT=Path(__file__).resolve().parent.parent
PREVIEW=ROOT/'build/m4-board-preview'

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare-only',action='store_true')
    args=parser.parse_args()
    PREVIEW.mkdir(parents=True,exist_ok=True)
    shutil.copytree(ROOT/'src',PREVIEW/'src',dirs_exist_ok=True)
    project=PREVIEW/'project'
    project.mkdir(exist_ok=True)
    for source in (ROOT/'project').iterdir():
        if source.is_file() and source.name!='.sconsign.dblite':
            shutil.copy2(source,project/source.name)
    for name in ['roll3_runtime.c','roll3_page.c']:
        path=PREVIEW/'src'/name
        text=path.read_text(encoding='utf-8')
        if 'r3_demo_world()' not in text:
            raise RuntimeError(f'{name}: expected world reference missing')
        text=text.replace('r3_demo_world()','r3_m4_world()')
        if name=='roll3_runtime.c':
            text=text.replace('build=m3-english-20261112','build=m4-compact-pillars-20261113')
        path.write_text(text,encoding='utf-8')
    print(f'Prepared dedicated M4 compact pillars preview: {PREVIEW}',flush=True)
    if args.prepare_only:
        return 0
    # Use the unmodified course entry against the explicitly isolated source tree.
    scripts=ROOT/'scripts'
    sys.path.insert(0,str(scripts))
    import _common
    config=json.loads((ROOT/'course.config.json').read_text(encoding='utf-8'))
    _common.ROOT=PREVIEW
    _common.load_config=lambda:config
    spec=importlib.util.spec_from_file_location('m4_course_build',scripts/'build_firmware.py')
    module=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    result=module.main([])
    if result==0:
        # SDK template's plain `cd` does not switch drives when launched from C:.
        for script in project.glob('build_*/uart_download.bat'):
            text=script.read_text(encoding='utf-8')
            text=text.replace('cd %WORK_PATH%', 'cd /d "%WORK_PATH%"')
            for name in ['bootloader\\bootloader.bin','main.bin','ftab\\ftab.bin']:
                text=text.replace('"'+name+'@','"%WORK_PATH%'+name+'@')
            script.write_text(text,encoding='utf-8')
    return result

if __name__=='__main__':
    raise SystemExit(main())
