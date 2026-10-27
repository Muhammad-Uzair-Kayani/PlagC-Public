#!/usr/bin/env python3
"""Acquire hash-pinned CC0 human/grass sources, assemble and validate the runtime human."""
import argparse, concurrent.futures, hashlib, json, os, pathlib, subprocess, sys, tempfile, urllib.request
ROOT=pathlib.Path(__file__).resolve().parents[1]
LOCK=pathlib.Path('Assets/Manifests/Landscape.lock.json')
RECEIPT=pathlib.Path('Assets/Cooked/TPS/Landscape/build-receipt.json')
HUMAN=pathlib.Path('Assets/Cooked/TPS/Human.plasset')
def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def recipe(root=ROOT):
    return hashlib.sha256(b''.join((root/p).read_bytes() for p in [LOCK,pathlib.Path('Scripts/PrepareHuman.py'),pathlib.Path('Assets/Source/TPS/Rifle.glb')])).hexdigest()
def fetch(entry,offline):
    path=(ROOT/entry['path']).resolve()
    if not path.is_relative_to(ROOT.resolve()):raise ValueError('Unsafe landscape source path')
    if path.is_file() and path.stat().st_size==entry['bytes'] and digest(path)==entry['sha256']:return
    if offline:raise ValueError('Missing/changed landscape source: '+str(path))
    path.parent.mkdir(parents=True,exist_ok=True)
    for attempt in range(3):
        fd,name=tempfile.mkstemp(dir=path.parent,suffix='.part')
        try:
            request=urllib.request.Request(entry['url'],headers={'User-Agent':'PlagC-AssetBuilder/1.0'})
            with os.fdopen(fd,'wb') as out,urllib.request.urlopen(request,timeout=90) as stream:
                while chunk:=stream.read(1024*1024):out.write(chunk)
            temporary=pathlib.Path(name)
            if temporary.stat().st_size!=entry['bytes'] or digest(temporary)!=entry['sha256']:raise ValueError('Source checksum mismatch: '+entry['path'])
            os.replace(temporary,path);print('Fetched',entry['path'],flush=True);return
        except Exception:
            if attempt==2:raise
        finally:pathlib.Path(name).unlink(missing_ok=True)
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--cooker',required=True,type=pathlib.Path);parser.add_argument('--offline',action='store_true');parser.add_argument('--rebuild',action='store_true');args=parser.parse_args()
    files=json.loads((ROOT/LOCK).read_text())['files']
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:list(pool.map(lambda entry:fetch(entry,args.offline),files))
    rifle=next(e for e in json.loads((ROOT/'Assets/Source/TPS/manifest.json').read_text()) if e['file']=='Rifle.glb')
    if digest(ROOT/'Assets/Source/TPS/Rifle.glb')!=rifle['sha256']:raise ValueError('Rifle source checksum mismatch')
    expected=recipe();receipt={}
    try:receipt=json.loads((ROOT/RECEIPT).read_text())
    except (OSError,ValueError):pass
    if not args.rebuild and receipt.get('recipe_sha256')==expected and (ROOT/HUMAN).is_file() and receipt.get('human_sha256')==digest(ROOT/HUMAN):
        print('Reused validated human asset');return
    output=ROOT/'Bin-Int/TPS/Human.glb'
    subprocess.run([sys.executable,str(ROOT/'Scripts/PrepareHuman.py'),'--output',str(output)],cwd=ROOT,check=True)
    target=ROOT/HUMAN;target.parent.mkdir(parents=True,exist_ok=True);temporary=target.with_suffix('.plasset.part')
    try:
        subprocess.run([str(args.cooker.resolve()),str(output),str(temporary)],cwd=ROOT,check=True)
        subprocess.run([str(args.cooker.resolve()),'--inspect',str(temporary)],cwd=ROOT,check=True)
        os.replace(temporary,target)
    finally:temporary.unlink(missing_ok=True)
    receipt={'recipe_sha256':expected,'lock_sha256':digest(ROOT/LOCK),'human_sha256':digest(target)}
    p=ROOT/RECEIPT;p.parent.mkdir(parents=True,exist_ok=True);temp=p.with_suffix('.part');temp.write_text(json.dumps(receipt,indent=2)+'\n');os.replace(temp,p)
if __name__=='__main__':
    try:main()
    except Exception as e:print('Landscape asset build failed:',e,file=sys.stderr);sys.exit(1)
