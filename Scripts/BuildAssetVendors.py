#!/usr/bin/env python3
"""CPU validation build mirroring Premake's pinned source manifest; never invokes CMake."""
import concurrent.futures, hashlib, json, os, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];os.chdir(root)
out=root/'Bin-Int/AssetTests';out.mkdir(parents=True,exist_ok=True)
manifest=json.loads((root/'Build/AssetVendors.json').read_text())
for name,config in manifest.items():
 flags=['-std=c++20','-O2','-DNDEBUG','-fPIC','-pthread']+['-I'+p for p in config['includes']]+['-D'+d for d in config['defines']]
 sources=config['sources'][:]
 vendor={'Assimp':'assimp','Ozz':'ozz','OpenAL':'openal'}[name]
 revision=subprocess.check_output(['git','-C','Vendor/'+vendor,'rev-parse','HEAD'],text=True).strip()
 manifest_hash=hashlib.sha256(json.dumps(config,sort_keys=True).encode()).hexdigest()
 if name=='OpenAL': sources.append('Vendor/openal/alc/backends/oss.cpp');flags+=['-DHAVE_OSS=1']
 def compile(source):
  obj=out/(name+'-'+hashlib.sha1(source.encode()).hexdigest()+'.o')
  signature=out/(obj.name+'.flags');key=' '.join(flags)+' '+revision+' '+manifest_hash
  headers=max((p.stat().st_mtime for p in (root/'Build/Config').rglob('*.h')),default=0)
  if not obj.exists() or obj.stat().st_mtime<max(Path(source).stat().st_mtime,headers) or not signature.exists() or signature.read_text()!=key:
   subprocess.run([os.environ.get('CXX','g++'),*flags,'-c',source,'-o',str(obj)],check=True);signature.write_text(key)
  return str(obj)
 with concurrent.futures.ThreadPoolExecutor(int(os.environ.get('BUILD_JOBS','3'))) as pool:objects=list(pool.map(compile,sources))
 if name=='OpenAL':subprocess.run(['g++','-shared',*objects,'-pthread','-ldl','-o',str(out/'libOpenAL.so')],check=True)
 else:subprocess.run(['ar','rcs',str(out/('lib'+name+'.a')),*objects],check=True)
 print(name+' built',flush=True)
