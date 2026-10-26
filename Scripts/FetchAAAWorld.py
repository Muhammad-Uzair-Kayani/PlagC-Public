#!/usr/bin/env python3
"""Fetch verified high-fidelity CC0 Poly Haven models for OUTPOST ZERO."""
from __future__ import annotations
import hashlib, json, pathlib, sys, urllib.request
ROOT=pathlib.Path(__file__).resolve().parents[1]; SOURCE=ROOT/"Assets"/"Source"/"AAA"; MANIFEST=SOURCE/"cook_manifest.txt"
API="https://api.polyhaven.com"; UA="PlagC-AssetPipeline/2.0 (Powered by Poly Haven)"
# Environment kit: architecture, utilities, security, lighting and micro-props. Runtime binaries are
# downloaded/cooked locally rather than committed, keeping git small while provenance stays exact.
ASSETS=[
("old_military_crate","Assets/Cooked/TPS/Crate.plasset"),("portable_generator","Assets/Cooked/TPS/Generator.plasset"),
("security_camera_01","Assets/Cooked/TPS/AAA/SecurityCamera.plasset"),("security_camera_02","Assets/Cooked/TPS/AAA/SecurityCamera02.plasset"),
("modular_factory_facade","Assets/Cooked/TPS/AAA/FactoryFacade.plasset"),("modular_industrial_pipes_01","Assets/Cooked/TPS/AAA/IndustrialPipes.plasset"),
("modular_pipes","Assets/Cooked/TPS/AAA/ModularPipes.plasset"),("steel_frame_shelves_01","Assets/Cooked/TPS/AAA/SteelShelves.plasset"),
("exterior_aircon_unit","Assets/Cooked/TPS/AAA/ExteriorAircon.plasset"),("utility_box_01","Assets/Cooked/TPS/AAA/UtilityBox.plasset"),
("barrel_03","Assets/Cooked/TPS/AAA/Barrel.plasset"),("plastic_crate_02","Assets/Cooked/TPS/AAA/PlasticCrate.plasset"),
("modular_airduct_rectangular_01","Assets/Cooked/TPS/AAA/Airduct.plasset"),("modular_airduct_circular_01","Assets/Cooked/TPS/AAA/CircularAirduct.plasset"),
("modular_electric_cables","Assets/Cooked/TPS/AAA/ElectricCables.plasset"),("modular_chainlink_fence","Assets/Cooked/TPS/AAA/ChainlinkFence.plasset"),
("concrete_road_barrier","Assets/Cooked/TPS/AAA/RoadBarrier.plasset"),("metal_jerrycan","Assets/Cooked/TPS/AAA/Jerrycan.plasset"),
("industrial_wall_lamp","Assets/Cooked/TPS/AAA/WallLamp.plasset"),("industrial_caged_sconce","Assets/Cooked/TPS/AAA/CagedSconce.plasset"),
("mounted_fluorescent_lights","Assets/Cooked/TPS/AAA/FluorescentLights.plasset"),("fire_alarm","Assets/Cooked/TPS/AAA/FireAlarm.plasset"),
("metal_toolbox","Assets/Cooked/TPS/AAA/Toolbox.plasset"),("hand_truck","Assets/Cooked/TPS/AAA/HandTruck.plasset"),
]
def get_json(url):
 req=urllib.request.Request(url,headers={"User-Agent":UA});
 with urllib.request.urlopen(req,timeout=60) as r:return json.load(r)
def nodes(n,k=()):
 if isinstance(n,dict):
  if isinstance(n.get("url"),str):yield k,n
  for a,b in n.items():
   if a not in {"url","md5","size"}:yield from nodes(b,k+(str(a),))
 elif isinstance(n,list):
  for i,b in enumerate(n):yield from nodes(b,k+(str(i),))
def choose(files):
 c=[]
 for k,n in nodes(files):
  ext=pathlib.PurePosixPath(n["url"].split("?",1)[0]).suffix.lower()
  if ext not in {".gltf",".glb"}:continue
  s="/".join(k).lower(); score=(300 if "2k" in s else 200 if "1k" in s else 100)+(20 if ext==".gltf" else 10)
  c.append((score,k,n))
 if not c:raise RuntimeError("asset has no glTF/GLB")
 return max(c,key=lambda x:x[0])[1:]
def download(url,dst,md5=None):
 if dst.exists() and (not md5 or hashlib.md5(dst.read_bytes()).hexdigest().lower()==md5.lower()):print(" cached",dst.name);return
 dst.parent.mkdir(parents=True,exist_ok=True); print(" download",dst.name); req=urllib.request.Request(url,headers={"User-Agent":UA})
 with urllib.request.urlopen(req,timeout=240) as r,open(dst,"wb") as f:
  while True:
   b=r.read(1024*1024)
   if not b:break
   f.write(b)
 if md5 and hashlib.md5(dst.read_bytes()).hexdigest().lower()!=md5.lower():dst.unlink(missing_ok=True);raise RuntimeError("checksum mismatch "+dst.name)
def main():
 SOURCE.mkdir(parents=True,exist_ok=True); cook=[]; evidence=[]
 for slug,out in ASSETS:
  print("[CINEMATIC]",slug); info=get_json(f"{API}/info/{slug}"); files=get_json(f"{API}/files/{slug}"); keys,primary=choose(files); package=SOURCE/slug; branch=files
  try:
   for key in keys[:-1]:branch=branch[key]
  except (KeyError,TypeError):branch=primary
  ns=list(nodes(branch)) or [(keys,primary)]; primary_path=None
  for _,n in ns:
   url=n["url"]; dst=package/pathlib.PurePosixPath(url.split("?",1)[0]).name; download(url,dst,n.get("md5"))
   if url==primary["url"]:primary_path=dst
  if primary_path is None:
   url=primary["url"];primary_path=package/pathlib.PurePosixPath(url.split("?",1)[0]).name;download(url,primary_path,primary.get("md5"))
  cook.append(f"{primary_path.relative_to(ROOT)}|{out}"); evidence.append({"slug":slug,"name":info.get("name",slug),"authors":info.get("authors",{}),"source":f"https://polyhaven.com/a/{slug}","license":"CC0 1.0 Universal","files_hash":info.get("files_hash","")})
 MANIFEST.write_text("\n".join(cook)+"\n",encoding="utf-8");(SOURCE/"provenance.json").write_text(json.dumps(evidence,indent=2),encoding="utf-8");print("Prepared",len(cook),"CC0 cinematic packages")
if __name__=="__main__":
 try:main()
 except Exception as e:print("AAA fetch failed:",e,file=sys.stderr);sys.exit(1)
