#!/usr/bin/env bash
# Actual pinned Jolt + ECS tests, no Ogre/window dependency. Run after FetchDependencies.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 - <<'PY'
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import os, subprocess
root=Path('Vendor/jolt/Jolt')
if not (root/'Jolt.h').exists(): raise SystemExit('Missing Jolt: run FetchDependencies.sh first')
out=Path('Bin-Int/PhysicsTests');out.mkdir(parents=True,exist_ok=True)
sources=sorted(p for p in root.rglob('*.cpp') if 'ObjectStream' not in p.parts or p.name=='SerializableObject.cpp')
compiler=os.environ.get('CXX','g++')
flags=['-std=c++20','-O2','-DNDEBUG','-DJPH_DISABLE_CUSTOM_ALLOCATOR','-pthread','-IVendor/jolt']
# Header changes invalidate all objects; compiler changes invalidate the archive.
headers=max(p.stat().st_mtime for p in root.rglob('*') if p.suffix in ('.h','.inl'))
signature=subprocess.check_output([compiler,'--version'],text=True)+' '.join(flags)
stamp=out/'compiler.txt';same=stamp.exists() and stamp.read_text()==signature
lib=out/'libJolt.a'
def build(p):
    obj=out/('_'.join(p.parts[2:])+'.o')
    if not same or not obj.exists() or obj.stat().st_mtime<max(headers,p.stat().st_mtime):
        subprocess.run([compiler,*flags,'-c',str(p),'-o',str(obj)],check=True)
    return str(obj)
with ThreadPoolExecutor(int(os.environ.get('BUILD_JOBS','2'))) as pool: objects=list(pool.map(build,sources))
subprocess.run(['ar','rcs',str(lib),*objects],check=True);stamp.write_text(signature)
test_flags=flags+(['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer'] if os.environ.get('PHYSICS_SANITIZE')=='1' else [])
test_exe=out/('tests-asan' if os.environ.get('PHYSICS_SANITIZE')=='1' else 'tests')
subprocess.run([compiler,*test_flags,'-DPC_PHYSICS_TEST_MAIN','-DPC_PHYSICS_RECORDING_TEST','-IPlagC/include','-IBuild/Pch','-IVendor/entt/src',
    'PlagC/src/Physics/PhysicsTypes.cpp','PlagC/src/Physics/PhysicsShapes.cpp','PlagC/src/Jobs.cpp','PlagC/src/Physics/JoltPhysicsWorld.cpp','PlagC/src/Physics/PhysicsSystem.cpp',
    'PlagC/src/World/World.cpp','PlagC/src/World/TransformSystem.cpp','PlagC/src/World/SceneRenderer.cpp',
    'Scripts/Tests/RecordingRenderer.cpp','Sandbox/src/PhysicsTests.cpp','Sandbox/src/PhysicsExtendedTests.cpp',str(lib),'-o',str(test_exe)],check=True)
subprocess.run([str(test_exe)],check=True)
PY
