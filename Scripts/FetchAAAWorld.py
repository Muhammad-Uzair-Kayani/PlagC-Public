#!/usr/bin/env python3
"""Fetch verified high-fidelity CC0 Poly Haven models for OUTPOST ZERO."""
from __future__ import annotations
import hashlib, json, pathlib, sys, urllib.request
ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCE = ROOT / "Assets" / "Source" / "AAA"
MANIFEST = SOURCE / "cook_manifest.txt"
API = "https://api.polyhaven.com"
UA = "PlagC-AssetPipeline/1.0 (Powered by Poly Haven)"
# Verified Poly Haven model slugs. First two replace the existing gameplay props.
ASSETS = [
    ("old_military_crate", "Assets/Cooked/TPS/Crate.plasset"),
    ("portable_generator", "Assets/Cooked/TPS/Generator.plasset"),
    ("security_camera_01", "Assets/Cooked/TPS/AAA/SecurityCamera.plasset"),
    ("modular_factory_facade", "Assets/Cooked/TPS/AAA/FactoryFacade.plasset"),
    ("modular_industrial_pipes_01", "Assets/Cooked/TPS/AAA/IndustrialPipes.plasset"),
    ("steel_frame_shelves_01", "Assets/Cooked/TPS/AAA/SteelShelves.plasset"),
    ("exterior_aircon_unit", "Assets/Cooked/TPS/AAA/ExteriorAircon.plasset"),
    ("utility_box_01", "Assets/Cooked/TPS/AAA/UtilityBox.plasset"),
    ("barrel_03", "Assets/Cooked/TPS/AAA/Barrel.plasset"),
    ("plastic_crate_02", "Assets/Cooked/TPS/AAA/PlasticCrate.plasset"),
    ("modular_airduct_rectangular_01", "Assets/Cooked/TPS/AAA/Airduct.plasset"),
]
def get_json(url):
    req=urllib.request.Request(url,headers={"User-Agent":UA})
    with urllib.request.urlopen(req,timeout=60) as r:return json.load(r)
def file_nodes(node,keys=()):
    if isinstance(node,dict):
        if isinstance(node.get("url"),str):yield keys,node
        for k,v in node.items():
            if k not in {"url","md5","size"}:yield from file_nodes(v,keys+(str(k),))
    elif isinstance(node,list):
        for i,v in enumerate(node):yield from file_nodes(v,keys+(str(i),))
def choose_model(files):
    c=[]
    for keys,node in file_nodes(files):
        ext=pathlib.PurePosixPath(node["url"].split("?",1)[0]).suffix.lower()
        if ext not in {".gltf",".glb"}:continue
        joined="/".join(keys).lower()
        score=(100 if "1k" in joined else 0)+(20 if ext==".gltf" else 10)
        c.append((score,keys,node))
    if not c:raise RuntimeError("asset has no glTF/GLB variant")
    return max(c,key=lambda x:x[0])[1:]
def download(url,dst,md5=None):
    if dst.exists() and md5 and hashlib.md5(dst.read_bytes()).hexdigest().lower()==md5.lower():
        print("  cached",dst.name);return
    dst.parent.mkdir(parents=True,exist_ok=True);print("  download",dst.name)
    req=urllib.request.Request(url,headers={"User-Agent":UA})
    with urllib.request.urlopen(req,timeout=180) as r,open(dst,"wb") as f:
        while True:
            b=r.read(1024*1024)
            if not b:break
            f.write(b)
    if md5 and hashlib.md5(dst.read_bytes()).hexdigest().lower()!=md5.lower():
        dst.unlink(missing_ok=True);raise RuntimeError("checksum mismatch for "+dst.name)
def main():
    SOURCE.mkdir(parents=True,exist_ok=True);cook=[];evidence=[]
    for slug,cooked in ASSETS:
        print("[AAA]",slug);info=get_json(f"{API}/info/{slug}");files=get_json(f"{API}/files/{slug}")
        keys,primary=choose_model(files);package=SOURCE/slug;branch=files
        try:
            for key in keys[:-1]:branch=branch[key]
        except (KeyError,TypeError):branch=primary
        nodes=list(file_nodes(branch)) or [(keys,primary)];primary_path=None
        for _,node in nodes:
            url=node["url"];dst=package/pathlib.PurePosixPath(url.split("?",1)[0]).name
            download(url,dst,node.get("md5"))
            if node is primary or url==primary["url"]:primary_path=dst
        if primary_path is None:
            url=primary["url"];primary_path=package/pathlib.PurePosixPath(url.split("?",1)[0]).name;download(url,primary_path,primary.get("md5"))
        cook.append(f"{primary_path.relative_to(ROOT)}|{cooked}")
        evidence.append({"slug":slug,"name":info.get("name",slug),"authors":info.get("authors",{}),"source":f"https://polyhaven.com/a/{slug}","license":"CC0 1.0 Universal","files_hash":info.get("files_hash","")})
    MANIFEST.write_text("\n".join(cook)+"\n",encoding="utf-8")
    (SOURCE/"provenance.json").write_text(json.dumps(evidence,indent=2),encoding="utf-8")
    print(f"Prepared {len(cook)} verified CC0 model packages.")
if __name__=="__main__":
    try:main()
    except Exception as exc:print("AAA asset fetch failed:",exc,file=sys.stderr);sys.exit(1)
