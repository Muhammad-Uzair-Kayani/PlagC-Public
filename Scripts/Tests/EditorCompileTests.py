#!/usr/bin/env python3
"""Compile editor translation units and the Ogre UI adapter with pinned headers.

Uses GCC/Clang syntax checks; native MSVC builds remain Windows acceptance.
"""
import argparse
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--vendor-root', type=Path, default=root / 'Vendor')
parser.add_argument('--imgui-root', type=Path)
args = parser.parse_args()
vendor = args.vendor_root.resolve()
imgui = (args.imgui_root or vendor / 'imgui').resolve()
ogre = vendor / 'ogre-next/OgreMain/include'
for header in [imgui / 'imgui.h', ogre / 'OgrePrerequisites.h']:
    if not header.is_file():
        parser.error(f'Missing pinned dependency: {header}')
flags = [os.environ.get('CXX', 'c++'), '-std=c++20', '-fsyntax-only',
         '-I' + str(root / 'PlagC/include'), '-I' + str(root / 'Build/Pch'),
         '-I' + str(root / 'Build/OgreConfig'), '-isystem', str(imgui),
         '-isystem', str(ogre), '-isystem', str(vendor / 'spdlog/include')]
sources = sorted((root / 'Editor/src').glob('*.cpp'))
sources.append(root / 'PlagC/src/Renderer/OgreUiBackend.cpp')
for source in sources:
    subprocess.run([*flags, str(source)], check=True)
    print(f'PASS {source.relative_to(root)}', flush=True)
