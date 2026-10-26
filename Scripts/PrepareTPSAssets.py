#!/usr/bin/env python3
"""Combine pinned CC0 humanoid clips and a rigid rifle into one reproducible GLB.
Original source files are immutable; output is cooked by AssetCooker.
"""
import copy, hashlib, json, struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'Assets/Source/TPS'
def read(name):
    data=(SOURCE/name).read_bytes()
    length=struct.unpack_from('<I',data,12)[0]
    doc=json.loads(data[20:20+length])
    binary=data[28+length:]
    return doc,bytearray(binary)
def main():
    for entry in json.loads((SOURCE/'manifest.json').read_text()):
        assert hashlib.sha256((SOURCE/entry['file']).read_bytes()).hexdigest()==entry['sha256'],entry['file']
    dest,binary=read('Mannequin.glb')
    names={n.get('name'):i for i,n in enumerate(dest['nodes'])}
    dest['animations']=[]
    def append_storage(src,raw):
        while len(binary)%4:binary.append(0)
        offset=len(binary);binary.extend(raw)
        views=len(dest.setdefault('bufferViews',[]));accessors=len(dest.setdefault('accessors',[]))
        for item in src.get('bufferViews',[]):
            v=copy.deepcopy(item);v['buffer']=0;v['byteOffset']=offset+v.get('byteOffset',0);dest['bufferViews'].append(v)
        for item in src.get('accessors',[]):
            v=copy.deepcopy(item)
            if 'bufferView' in v:v['bufferView']+=views
            assert 'sparse' not in v,'sparse accessor unsupported'
            dest['accessors'].append(v)
        return views,accessors
    for filename,clips in [('General.glb',{'Idle_A':'Idle','Hit_A':'Hit','Death_A':'Death'}),('Movement.glb',{'Walking_A':'Walk','Running_A':'Run','Jump_Idle':'Jump'}),('Combat.glb',{'Ranged_2H_Aiming':'Aim','Ranged_2H_Shooting':'Fire','Ranged_2H_Reload':'Reload'})]:
        src,raw=read(filename);_,offset=append_storage(src,raw)
        for a in src['animations']:
            if a['name'] not in clips:continue
            a=copy.deepcopy(a);a['name']=clips[a['name']]
            for s in a['samplers']:s['input']+=offset;s['output']+=offset
            a['channels']=[c for c in a['channels'] if src['nodes'][c['target']['node']]['name'] in names]
            for c in a['channels']:c['target']['node']=names[src['nodes'][c['target']['node']]['name']]
            dest['animations'].append(a)
    src,raw=read('Rifle.glb');views,accessors=append_storage(src,raw)
    offsets={k:len(dest.setdefault(k,[])) for k in ['images','samplers','textures','materials','meshes','nodes']}
    for im in src.get('images',[]):
        im=copy.deepcopy(im);im['bufferView']+=views;dest['images'].append(im)
    dest['samplers'].extend(src.get('samplers',[]))
    for t in src.get('textures',[]):
        t=copy.deepcopy(t);t['source']+=offsets['images']
        if 'sampler' in t:t['sampler']+=offsets['samplers']
        dest['textures'].append(t)
    def textures(value):
        if isinstance(value,dict):
            for key,item in value.items():
                if key.endswith('Texture') and isinstance(item,dict):item['index']+=offsets['textures']
                else:textures(item)
    for m in src.get('materials',[]):
        m=copy.deepcopy(m);textures(m);dest['materials'].append(m)
    for m in src['meshes']:
        m=copy.deepcopy(m)
        for p in m['primitives']:
            p['attributes']={k:v+accessors for k,v in p['attributes'].items()}
            if 'indices' in p:p['indices']+=accessors
            if 'material' in p:p['material']+=offsets['materials']
        dest['meshes'].append(m)
    for n in src['nodes']:
        n=copy.deepcopy(n);n['name']='Rifle_'+n.get('name','Node')
        if 'mesh' in n:n['mesh']+=offsets['meshes']
        if 'children' in n:n['children']=[i+offsets['nodes'] for i in n['children']]
        dest['nodes'].append(n)
    mount=len(dest['nodes'])
    dest['nodes'].append({'name':'RifleMount','translation':[0,-.20,-.0575],'rotation':[-.5,.5,.5,.5],'scale':[.65]*3,'children':[i+offsets['nodes'] for i in src['scenes'][src.get('scene',0)]['nodes']]})
    dest['nodes'][names['hand.r']].setdefault('children',[]).append(mount)
    uniform=len(dest['materials']);dest['materials'].append({'name':'NavyUniform','pbrMetallicRoughness':{'baseColorFactor':[.09,.15,.20,1],'metallicFactor':0,'roughnessFactor':.8}})
    for n in dest['nodes'][:offsets['nodes']]:
        if 'mesh' in n:
            for p in dest['meshes'][n['mesh']]['primitives']:
                if 'Head' not in n.get('name',''):p['material']=uniform
                else:
                    helmet=copy.deepcopy(dest['materials'][p['material']]);helmet['name']='Helmet'
                    helmet.setdefault('pbrMetallicRoughness',{})['baseColorFactor']=[.24,.3,.38,1]
                    p['material']=len(dest['materials']);dest['materials'].append(helmet)
    dest['buffers']=[{'byteLength':len(binary)}]
    encoded=json.dumps(dest,separators=(',',':')).encode();encoded+=b' '*((-len(encoded))%4)
    binary+=b'\0'*((-len(binary))%4)
    data=struct.pack('<4sII',b'glTF',2,28+len(encoded)+len(binary))+struct.pack('<I4s',len(encoded),b'JSON')+encoded+struct.pack('<I4s',len(binary),b'BIN\0')+binary
    out=ROOT/'Bin-Int/TPS/Operative.glb';out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(data)
    print(out)
if __name__=='__main__':main()
