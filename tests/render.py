#!/usr/bin/env python3
"""Compile the real UI against SDK LVGL, run interaction checks, write PPM previews."""
import argparse
import concurrent.futures
import hashlib
import os
import shlex
from pathlib import Path
import subprocess
import wave
import sys

parser = argparse.ArgumentParser()
parser.add_argument('--sdk', required=True, type=Path)
parser.add_argument('--out', type=Path, default=Path('/tmp/peripheral-lab-preview'))
parser.add_argument('--maze-only', action='store_true', help='B2-1 simulated maze preview checks only')
parser.add_argument('--ui-only', action='store_true', help='Run Lab UI integration checks without unrelated service tests')
parser.add_argument('--roll3-view', action='store_true', help='B 3D renderer and Chinese synthetic view tests')
parser.add_argument('--runtime-ui', action='store_true', help='Real runtime and real LVGL connection tests')
args = parser.parse_args()
root = Path(__file__).resolve().parent
lvgl = args.sdk.resolve() / 'external/lvgl_v9'
out = args.out.resolve()
out.mkdir(parents=True, exist_ok=True)
if not args.maze_only and not args.runtime_ui and not args.roll3_view:
    subprocess.run([sys.executable, str(root.parent / 'tools/generate_game_audio.py'), '--check'], check=True)
cc = os.environ.get('CC', 'cc')
platform_flags = shlex.split(os.environ.get('CFLAGS', ''))
flags = platform_flags + ['-std=c11', '-O1', '-DLV_KCONFIG_IGNORE', '-DLV_CONF_INCLUDE_SIMPLE',
         '-I' + str(root / 'include'), '-I' + str(root.parent / 'src'), '-I' + str(lvgl)]
sources = sorted((lvgl / 'src').rglob('*.c'))
app_sources = ('maze_menu.c', 'maze_ui.c', 'maze_ui_runtime.c', 'maze_levels.c', 'maze_game.c',
               'maze_font_16.c', 'maze_font_20.c', 'maze_font_28.c')
if args.runtime_ui:
    flags.insert(0, '-I' + str(root / 'ui_runtime_stubs'))
    app_sources += ('maze_runtime.c', 'maze_controller.c')
else:
    sources.append(root / 'maze_runtime_unavailable.c')
if not args.maze_only and not args.runtime_ui and not args.roll3_view:
    app_sources += ('lab_game_ui.c', 'lab_ride.c', 'lab_ui.c', 'lab_ble_ui.c', 'lab_pan_ui.c', 'lab_registry.c', 'modules/test_display.c', 'modules/test_touch.c', 'modules/touch_decode.c')
if args.roll3_view:
    app_sources=('roll3_renderer.c','roll3_view.c','roll3_page.c','roll3_physics.c',
                 'roll3_scene.c','roll3_font_16.c','roll3_font_20.c')
    sources=[p for p in sources if p.name!='maze_runtime_unavailable.c']
sources += [root.parent / 'src' / name for name in app_sources]
sources.append(root / ('roll3_view_test.c' if args.roll3_view else 'maze_ui_runtime_test.c' if args.runtime_ui else 'maze_ui_test.c' if args.maze_only else 'ui_test.c'))
headers = list((lvgl / 'src').rglob('*.h')) + list((root / 'include').glob('*.h'))
headers += list((root.parent / 'src').glob('*.h'))
header_mtime = max(p.stat().st_mtime for p in headers)

def compile_one(item):
    _, source = item
    source_flags = flags
    if args.roll3_view and source.name == 'roll3_view.c':
        source_flags = flags + ['-Dlv_malloc=roll3_test_malloc', '-Dlv_free=roll3_test_free']
    key = hashlib.sha256((str(source) + repr(source_flags) + cc).encode()).hexdigest()[:20]
    obj = out / f'{key}.o'
    # Rebuild after config changes too; cached SDK objects make UI iterations quick.
    newest = max(source.stat().st_mtime, header_mtime)
    if not obj.exists() or obj.stat().st_mtime < newest:
        result = subprocess.run([cc, *source_flags, '-c', str(source), '-o', str(obj)], capture_output=True, text=True)
        if result.returncode:
            raise RuntimeError(result.stderr)
    return str(obj)

with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
    objects = list(pool.map(compile_one, enumerate(sources)))
exe = out / 'ui-test'
# Keep the linker command below Windows' process command-line length limit.
# GCC response files accept quoted forward-slash paths, including spaces.
link_inputs = out / 'ui-link.rsp'
link_inputs.write_text('\n'.join('"' + Path(obj).as_posix() + '"' for obj in objects) + '\n', encoding='utf-8')
wraps = ['-Wl,--wrap=' + n for n in ('maze_runtime_snapshot','maze_runtime_next_request','maze_runtime_action','maze_runtime_init_status')] if args.runtime_ui else []
subprocess.run([cc, *platform_flags, '@' + str(link_inputs), *wraps, '-lm', '-o', str(exe)], check=True)
subprocess.run([str(exe)], cwd=out, check=True, timeout=30)
if args.maze_only or args.ui_only or args.runtime_ui or args.roll3_view:
    print(f'Host UI previews (simulated): {out}')
    sys.exit(0)
core = out / 'core-test'
subprocess.run([cc, '-I' + str(root / 'core_include'), *flags, '-pthread',
                str(root / 'core_test.c'), str(root.parent / 'src/lab.c'), '-o', str(core)], check=True)
subprocess.run([str(core)], check=True, timeout=10)
for name, extra in [('game_sound', []), ('game_sensor', ['modules/test_game.c']), ('ride', ['lab_ride.c']), ('units', ['lab_units.c']),
                    ('capture', ['modules/touch_capture.c', 'modules/touch_decode.c']),
                    ('audio', ['lab_audio.c', 'lab_wav.c', 'modules/test_wav.c']),
                    ('wav', ['lab_wav.c']),
                    ('pan', ['network/lab_pan_model.c']),
                    ('mqtt', ['network/lab_mqtt.c']),
                    ('ble', ['bluetooth/lab_ble_model.c'])]:
    test = out / (name + '-test')
    subprocess.run([cc, '-pthread', '-I' + str(root / 'core_include'), *flags,
                    str(root / (name + '_test.c')),
                    *[str(root.parent / 'src' / p) for p in extra], '-lm', '-o', str(test)], check=True)
    subprocess.run([str(test)], cwd=out if name == 'game_sound' else None, check=True, timeout=10)
    if name == 'game_sound':
        for cue in ('hit', 'win', 'lose'):
            with wave.open(str(out / f'game-{cue}.wav'), 'wb') as wav:
                wav.setparams((1, 2, 16000, 0, 'NONE', 'not compressed'))
                wav.writeframes((out / f'game-{cue}.pcm').read_bytes())
ble_service = out / 'ble-service-test'
subprocess.run([cc, '-I' + str(root / 'ble_include'), '-I' + str(root / 'core_include'), *flags,
                '-I' + str(args.sdk.resolve() / 'middleware/bluetooth/include'),
                str(root / 'ble_service_test.c'), str(root.parent / 'src/bluetooth/lab_ble_model.c'),
                '-o', str(ble_service)], check=True)
subprocess.run([str(ble_service)], check=True, timeout=10)
print(f'Previews: {out}')

pan_service = out / 'pan-service-test'
subprocess.run([cc, '-I' + str(root / 'pan_include'), *flags,
                str(root / 'pan_service_test.c'), '-o', str(pan_service)], check=True)
subprocess.run([str(pan_service)], check=True, timeout=10)

net_test = out / 'pan-net-test'
subprocess.run([cc, '-I' + str(root / 'net_include'), '-I' + str(root / 'pan_include'), *flags,
                str(root / 'pan_net_test.c'), str(root.parent / 'src/network/lab_mqtt.c'),
                str(root.parent / 'src/network/lab_pan_model.c'), '-o', str(net_test)], check=True)
subprocess.run([str(net_test)], check=True, timeout=10)
