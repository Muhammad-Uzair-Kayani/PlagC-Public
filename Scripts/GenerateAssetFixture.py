#!/usr/bin/env python3
"""Original two-joint glTF fixture with embedded texture/buffer; no third-party model assets."""
import base64,json,math,struct,zlib
from pathlib import Path
root=Path(__file__).resolve().parents[1]
data=bytearray();views=[];accessors=[]
def accessor(values,fmt,kind,component,minimum=None,maximum=None):
 while len(data)%4:data.append(0)
 offset=len(data)
 for value in values:data.extend(struct.pack('<'+fmt,*value))
 views.append({'buffer':0,'byteOffset':offset,'byteLength':len(data)-offset})
 a={'bufferView':len(views)-1,'componentType':component,'count':len(values),'type':kind}
 if minimum is not None:a.update(min=minimum,max=maximum)
 accessors.append(a);return len(accessors)-1
positions=[];normals=[];uvs=[];joints=[];weights=[];indices=[]
faces=[((0,0,1),[(-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)]),((0,0,-1),[(1,-1,-1),(-1,-1,-1),(-1,1,-1),(1,1,-1)]),((1,0,0),[(1,-1,1),(1,-1,-1),(1,1,-1),(1,1,1)]),((-1,0,0),[(-1,-1,-1),(-1,-1,1),(-1,1,1),(-1,1,-1)]),((0,1,0),[(-1,1,1),(1,1,1),(1,1,-1),(-1,1,-1)]),((0,-1,0),[(-1,-1,-1),(1,-1,-1),(1,-1,1),(-1,-1,1)])]
for joint,center,scale in [(0,(0,.65,0),(.5,.65,.4)),(1,(0,2.1,0),(.25,.6,.25))]:
 for normal,corners in faces:
  start=len(positions)
  for point,uv in zip(corners,[(0,0),(1,0),(1,1),(0,1)]):
   positions.append(tuple(center[i]+point[i]*scale[i] for i in range(3)));normals.append(normal);uvs.append(uv);joints.append((joint,0,0,0));weights.append((1,0,0,0))
  indices.extend((start+i,) for i in [0,1,2,0,2,3])
attrs={'POSITION':accessor(positions,'3f','VEC3',5126,[-.5,0,-.4],[.5,2.7,.4]),'NORMAL':accessor(normals,'3f','VEC3',5126),'TEXCOORD_0':accessor(uvs,'2f','VEC2',5126),'JOINTS_0':accessor(joints,'4H','VEC4',5123),'WEIGHTS_0':accessor(weights,'4f','VEC4',5126)}
idx=accessor(indices,'H','SCALAR',5123)
identity=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1];inverse=identity.copy();inverse[13]=-1.5
bind=accessor([identity,inverse],'16f','MAT4',5126)
time=accessor([(0,),(1,),(2,)],'f','SCALAR',5126,[0],[2])
rot=accessor([(0,0,math.sin(a/2),math.cos(a/2)) for a in [0,.8,0]],'4f','VEC4',5126)
def chunk(kind,payload):return struct.pack('>I',len(payload))+kind+payload+struct.pack('>I',zlib.crc32(kind+payload))
png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',2,2,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(bytes([0,40,160,230,255,240,180,30,255,0,240,180,30,255,40,160,230,255])))+chunk(b'IEND',b'')
uri=lambda kind,blob:'data:'+kind+';base64,'+base64.b64encode(blob).decode()
model={'asset':{'version':'2.0','generator':'PlagC original asset fixture'},'scene':0,'scenes':[{'nodes':[0]}],'nodes':[{'name':'Root','children':[1,3]},{'name':'BaseJoint','children':[2]},{'name':'ArmJoint','translation':[0,1.5,0]},{'name':'BeaconMesh','mesh':0,'skin':0}], 'meshes':[{'name':'Beacon','primitives':[{'attributes':attrs,'indices':idx,'material':0}]}],'skins':[{'joints':[1,2],'skeleton':1,'inverseBindMatrices':bind}],'animations':[{'name':'Wave','samplers':[{'input':time,'output':rot,'interpolation':'LINEAR'}],'channels':[{'sampler':0,'target':{'node':2,'path':'rotation'}}]}],'materials':[{'name':'CheckerMetal','pbrMetallicRoughness':{'baseColorTexture':{'index':0},'metallicFactor':.35,'roughnessFactor':.5}}],'textures':[{'source':0}],'images':[{'uri':uri('image/png',png)}],'buffers':[{'byteLength':len(data),'uri':uri('application/octet-stream',data)}],'bufferViews':views,'accessors':accessors}
folder=root/'Assets/Source';folder.mkdir(parents=True,exist_ok=True);(folder/'Beacon.gltf').write_text(json.dumps(model,indent=2)+'\n')
