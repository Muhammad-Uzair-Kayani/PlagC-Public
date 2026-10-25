"""Original embedded PNG maps to exercise every supported PBR channel."""
import json,base64,struct,zlib
from pathlib import Path
j=json.loads(Path('Assets/Source/Beacon.gltf').read_text())
def png(fn):
    def chunk(k,p):return struct.pack('>I',len(p))+k+p+struct.pack('>I',zlib.crc32(k+p))
    data=b''.join(b'\0'+bytes(v for x in range(16) for v in fn(x,y)) for y in range(16))
    return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',16,16,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(data))+chunk(b'IEND',b'')
functions=[lambda x,y:(180,90,30,255),lambda x,y:(160 if x%4<2 else 96,128,250,255),lambda x,y:(255,40+12*y,255 if x<8 else 0,255),lambda x,y:(60 if x<4 or y<4 else 255,)*3+(255,),lambda x,y:(255,60,10,255) if x in (7,8) else (0,0,0,255)]
j['images']=[{'uri':'data:image/png;base64,'+base64.b64encode(png(fn)).decode()} for fn in functions]
j['textures']=[{'source':i} for i in range(5)]
j['materials']=[{'name':'Phase55 PBR Test','pbrMetallicRoughness':{'baseColorTexture':{'index':0},'metallicRoughnessTexture':{'index':2},'metallicFactor':1,'roughnessFactor':1},'normalTexture':{'index':1,'scale':.8},'occlusionTexture':{'index':3,'strength':.8},'emissiveTexture':{'index':4},'emissiveFactor':[1,1,1]}]
j.pop('animations',None)
Path('Assets/Source/MaterialLab.gltf').write_text(json.dumps(j,indent=2)+'\n')
