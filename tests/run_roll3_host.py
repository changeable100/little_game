"""Run production 3D logic and runtime-boundary tests, preserving raw logs.

Native GCC/Clang: python tests/run_roll3_host.py --cc gcc
Isolated Zig: uv run --no-project --with ziglang==0.13.0 python tests/run_roll3_host.py --zig
No firmware flashing or device access. RT/LVGL transport in runtime suite is fake.
"""
import argparse
from datetime import datetime
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--cc', default='gcc')
parser.add_argument('--zig', action='store_true')
args = parser.parse_args()
compiler = [sys.executable, '-m', 'ziglang', 'cc'] if args.zig else [args.cc]
(root / 'build').mkdir(exist_ok=True)
evidence = root / 'evidence/M4/game-menu-records-2026-11-13/host'
evidence.mkdir(parents=True, exist_ok=True)
log = evidence / ('host_' + datetime.now().strftime('%Y-%m-%d_%H%M%S') + '.txt')
suffix = '.exe' if sys.platform == 'win32' else ''
suites = [
    ('roll3_level2_host', ['-Isrc'], ['src/roll3_physics.c','src/roll3_mesh.c','src/roll3_scene_level2.c','src/roll3_scene.c','src/roll3_scene_m4.c','src/roll3_level2_selftest.c','tests/roll3_level2_host.c']),
    ('roll3_main_host', ['-Itests/runtime_stubs', '-Isrc'], ['tests/roll3_main_host.c']),
    ('roll3_records_host', ['-Isrc'], ['src/roll3_records.c','src/roll3_records_selftest.c','tests/roll3_records_host.c']),
    ('roll3_records_service_host', ['-Itests/runtime_stubs','-Isrc'], ['src/roll3_records.c','tests/roll3_records_service_host.c']),
    ('roll3_host', ['-Isrc'], ['src/roll3_physics.c','src/roll3_mesh.c','src/roll3_scene_level2.c','src/roll3_scene.c','src/roll3_scene_m4.c','src/roll3_m4_selftest.c',
                             'src/roll3_selftest.c','src/roll3_level2_selftest.c','src/roll3_session.c','src/roll3_session_selftest.c','tests/roll3_host.c']),
    ('roll3_runtime_host', ['-Itests/runtime_stubs','-Isrc'], ['src/roll3_physics.c','src/roll3_mesh.c','src/roll3_scene_level2.c','src/roll3_scene.c','src/roll3_scene_m4.c',
                             'src/roll3_records.c','src/roll3_session.c','tests/roll3_runtime_host.c']),
]

with log.open('x', encoding='utf-8') as stream:
    def run(command):
        stream.write('COMMAND ' + subprocess.list2cmdline(command) + '\n')
        stream.flush()
        result = subprocess.run(command, cwd=root, capture_output=True, text=True,
                                encoding='utf-8', errors='replace')
        output = result.stdout + result.stderr
        stream.write(output + f'\nexit_code={result.returncode}\n')
        stream.flush()
        print(output, end='')
        if result.returncode:
            print(f'Failure retained: {log}')
            raise SystemExit(result.returncode)
    run(compiler + ['--version'])
    for name, includes, sources in suites:
        executable = root / 'build' / (name + suffix)
        run(compiler + ['-std=c11', '-Wall', '-Wextra', '-Werror', '-O2'] +
            includes + sources + ['-o', str(executable)])
        run([str(executable)])
print(f'Raw host evidence: {log}')
