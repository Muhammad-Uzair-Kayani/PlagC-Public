"""Derive a skeletal-only test GLB from the pinned CC0 RobotExpressive source.
Keeps geometry/materials/skin; removes facial morph targets and weight channels.
"""
from pathlib import Path
import json,struct,hashlib
source=Path('Assets/Source/RobotExpressive.glb')
raw=source.read_bytes()
assert hashlib.sha256(raw).hexdigest()=='047f5e5fb3bb6d378bd1df16ca6137f2a596c99b3a1b5690b4020c05aaf6f319'
size=struct.unpack_from('<I',raw,12)[0]
j=json.loads(raw[20:20+size])
for mesh in j['meshes']:
    mesh.pop('weights',None)
    for primitive in mesh['primitives']:primitive.pop('targets',None)
for index,node in enumerate(j['nodes']):
    node.pop('weights',None)
    node['name']=node.get('name','Node')+'_n'+str(index)
for clip in j['animations']:
    clip['channels']=[c for c in clip['channels'] if c['target']['path']!='weights']
text=json.dumps(j,separators=(',',':')).encode()
text+=b' '*((-len(text))%4)
binary=raw[20+size:]
output=struct.pack('<III',0x46546c67,2,20+len(text)+len(binary))+struct.pack('<II',len(text),0x4e4f534a)+text+binary
path=Path('Bin-Int/Phase55/RobotExpressive.glb');path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(output)
print(path)
