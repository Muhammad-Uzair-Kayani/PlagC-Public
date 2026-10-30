#!/usr/bin/env python3
"""Run the real ImGui input bridge without a window, renderer, or GPU."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--imgui-root', type=Path, default=root / 'Vendor/imgui')
args = parser.parse_args()
imgui = args.imgui_root.resolve()
sources = [root / 'Scripts/Tests/EditorInputTests.cpp',
           root / 'Editor/src/ImGuiInputBridge.cpp']
sources += [imgui / name for name in
            ('imgui.cpp', 'imgui_draw.cpp', 'imgui_widgets.cpp', 'imgui_tables.cpp')]
for source in sources:
    if not source.is_file():
        parser.error(f'Missing source: {source}. Fetch pinned dependencies first.')
with tempfile.TemporaryDirectory(prefix='plagc-editor-input-') as directory:
    executable = Path(directory) / ('input-tests.exe' if os.name == 'nt' else 'input-tests')
    command = [os.environ.get('CXX', 'c++'), '-std=c++20', '-O0', '-pthread',
               '-I' + str(root / 'PlagC/include'), '-I' + str(root / 'Build/Pch'),
               '-I' + str(imgui), *map(str, sources), '-o', str(executable)]
    subprocess.run(command, check=True)
    subprocess.run([str(executable)], cwd=directory, check=True)
print('PASS EditorInputTests')
