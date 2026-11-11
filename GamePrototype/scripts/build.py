"""Portable dependency-free GCC/Clang build fallback (CMake is recommended)."""
import os
import subprocess
from pathlib import Path
root = Path(__file__).resolve().parents[1]
os.chdir(root)
Path('build').mkdir(exist_ok=True)
common = [os.environ.get('CXX', 'g++'), '-std=c++20', '-O2', '-Wall', '-Wextra', '-Icpp/core/include']
core = ['cpp/core/src/game_session.cpp', 'cpp/core/src/simulation.cpp']
for target, sources in [('GameRuntime', core + ['cpp/core/src/synthetic.cpp', 'cpp/runtime/main.cpp', 'cpp/runtime/transport.cpp']), ('core_tests', core + ['cpp/tests/core_tests.cpp']), ('scenario_tests', core + ['cpp/core/src/synthetic.cpp', 'cpp/tests/scenario_tests.cpp'])]:
    subprocess.run(common + ['-Ithird_party'] + sources + ['-o', 'build/' + target], check=True)
