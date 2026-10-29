#!/usr/bin/env python3
"""Portable compiler checks for the editor backend; no renderer or prebuilt libs.
Run after fetching pinned EnTT/RapidJSON. Linux GCC/Clang runner.
"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument('--vendor-root', type=Path, default=root / 'Vendor')
parser.add_argument('--sanitize', action='store_true')
args = parser.parse_args()

flags = [
    '-std=c++20', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
    '-ffunction-sections', '-fdata-sections', '-pthread',
    '-I' + str(root / 'PlagC/include'), '-I' + str(root / 'Build/Pch'),
    '-isystem', str(args.vendor_root / 'entt/src'),
    '-isystem', str(args.vendor_root / 'rapidjson/include')
]
if args.sanitize:
    flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']

compiler = os.environ.get('CXX', 'c++')
link_sources = [
    'PlagC/src/World/World.cpp',
    'PlagC/src/Physics/PhysicsTypes.cpp',
    'PlagC/src/Editor/SceneDocument.cpp',
    'PlagC/src/Editor/SceneSerialization.cpp',
    'PlagC/src/Editor/EditorModels.cpp',
    'PlagC/src/Editor/PlaySession.cpp',
    'PlagC/src/Editor/EditorSession.cpp',
]
compile_only = [
    'PlagC/src/Editor/ViewportTarget.cpp',
    'Scripts/Tests/EditorBridgeCompile.cpp',
]
tests = [
    'EditorHistoryTests',
    'EditorDocumentTests',
    'EditorSerializationTests',
    'EditorModelTests',
    'EditorPlaySessionTests',
    'EditorSessionTests',
    'WorldHierarchyTests',
    'EditorHierarchyTests',
]

with tempfile.TemporaryDirectory(prefix='plagc-editor-tests-') as tmp:
    objects = []
    for i, source in enumerate(link_sources):
        obj = str(Path(tmp) / f'core-{i}.o')
        subprocess.run(
            [compiler, *flags, '-c', str(root / source), '-o', obj],
            check=True)
        objects.append(obj)

    for i, source in enumerate(compile_only):
        obj = str(Path(tmp) / f'compile-only-{i}.o')
        subprocess.run(
            [compiler, *flags, '-c', str(root / source), '-o', obj],
            check=True)
        print(f'PASS compile {source}', flush=True)

    for name in tests:
        exe = str(Path(tmp) / name)
        subprocess.run(
            [compiler, *flags, str(root / f'Scripts/Tests/{name}.cpp'),
             *objects, '-Wl,--gc-sections', '-o', exe],
            check=True)
        subprocess.run([exe], check=True)
        print(f'PASS {name}', flush=True)
