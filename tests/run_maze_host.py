"""Run production maze logic and runtime-boundary tests, preserving raw logs.

Native GCC/Clang: python tests/run_maze_host.py --cc gcc
Isolated Zig: uv run --no-project --with ziglang==0.13.0 python tests/run_maze_host.py --zig
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
evidence = root / 'evidence/M2/maze_adapter'
evidence.mkdir(parents=True, exist_ok=True)
log = evidence / ('host_' + datetime.now().strftime('%Y-%m-%d_%H%M%S') + '.txt')
suffix = '.exe' if sys.platform == 'win32' else ''
suites = [
    ('maze_adapter_host', ['-Isrc'], [
        'src/maze_game.c', 'src/maze_selftest.c', 'src/maze_levels.c',
        'src/maze_levels_selftest.c', 'src/maze_controller.c',
        'src/maze_controller_selftest.c', 'tests/maze_adapter_host.c']),
    ('maze_runtime_host', ['-Itests/runtime_stubs', '-Isrc'], [
        'src/maze_game.c', 'src/maze_levels.c', 'src/maze_controller.c',
        'src/maze_runtime.c', 'tests/maze_runtime_host.c']),
    ('maze_main_host', ['-Itests/runtime_stubs', '-Isrc'], ['tests/maze_main_host.c']),
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
