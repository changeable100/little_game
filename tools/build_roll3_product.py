"""Use the unchanged course build, then keep UART paths valid across drives."""
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parent.parent
def fix_downloads():
    for script in (ROOT/'project').glob('build_*/uart_download.bat'):
        text=script.read_text(encoding='utf-8')
        text=text.replace('cd %WORK_PATH%','cd /d "%WORK_PATH%"')
        for name in ['bootloader\\bootloader.bin','main.bin','ftab\\ftab.bin']:
            text=text.replace('"'+name+'@','"%WORK_PATH%'+name+'@')
        script.write_text(text,encoding='utf-8')
if __name__=='__main__':
    sys.path.insert(0,str(ROOT/'scripts'))
    import build_firmware
    result=build_firmware.main(sys.argv[1:])
    if not result:fix_downloads()
    raise SystemExit(result)
