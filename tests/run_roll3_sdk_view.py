"""Use installed Zig cc to run the real SDK LVGL checks for this snapshot."""
import os
from pathlib import Path
import runpy
import subprocess
import sys

root = Path(__file__).resolve().parent.parent
zig = Path(sys.executable).parent.parent / "Lib/site-packages/ziglang/zig.exe"
if not zig.is_file():
    raise SystemExit("Run with the Python environment containing ziglang")
original_run = subprocess.run

def run(command, *args, **kwargs):
    if isinstance(command, (list, tuple)) and command and command[0] == str(zig):
        command = [str(zig), "cc", *command[1:]]
    return original_run(command, *args, **kwargs)

subprocess.run = run
os.environ["CC"] = str(zig)
os.environ["CFLAGS"] = ""
driver = root / "tests/render.py"
sys.argv = [str(driver), "--sdk", "D:/SiFli-SDK/v2.5.1", "--roll3-view",
            "--out", str(root / "build/roll3-live-view")]
runpy.run_path(str(driver), run_name="__main__")
