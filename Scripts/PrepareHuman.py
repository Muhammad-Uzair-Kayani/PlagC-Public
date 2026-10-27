#!/usr/bin/env python3
"""Offline CC0 humanoid assembly: bind-pose-aware clips, uniform, rigid armor and weapon socket.
Only Python's standard library is required. Sources are verified by BuildLandscapeAssets.py.
"""
import copy, json, math, struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

def read(path):
    data=Path(path).read_bytes()
    if data[:4]!=b'glTF': raise ValueError('expected GLB')
    length=struct.unpack_from('<I',data,12)[0]
    return json.loads(data[20:20+length]),bytearray(data[28+length:])

def write(path,doc,binary):
    doc['buffers']=[{'byteLength':len(binary)}]
    encoded=json.dumps(doc,separators=(',',':')).encode();encoded+=b' '*((-len(encoded))%4)
    binary+=b'\0'*((-len(binary))%4)
    data=struct.pack('<4sII',b'glTF',2,28+len(encoded)+len(binary))+struct.pack('<I4s',len(encoded),b'JSON')+encoded+struct.pack('<I4s',len(binary),b'BIN\0')+binary
    Path(path).parent.mkdir(parents=True,exist_ok=True);Path(path).write_bytes(data)

def mul(a,b):
    x,y,z,w=a;X,Y,Z,W=b
    return [w*X+x*W+y*Z-z*Y,w*Y-x*Z+y*W+z*X,w*Z+x*Y-y*X+z*W,w*W-x*X-y*Y-z*Z]
def inv(q): return [-q[0],-q[1],-q[2],q[3]]
def rotate(q,v): return mul(mul(q,[*v,0]),inv(q))[:3]
def normalize(q):
    n=math.sqrt(sum(x*x for x in q));return [x/n for x in q]
def slerp(a,b,t):
    dot=sum(x*y for x,y in zip(a,b))
    if dot<0:b=[-x for x in b];dot=-dot
    if dot>.9995:return normalize([x+(y-x)*t for x,y in zip(a,b)])
    angle=math.acos(min(1,dot));s=math.sin(angle)
    return [(x*math.sin((1-t)*angle)+y*math.sin(t*angle))/s for x,y in zip(a,b)]

def accessor(doc,binary,index):
    a=doc['accessors'][index]
    formats={5126:'f',5125:'I',5123:'H',5121:'B'};components={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4,'MAT4':16}
    n=components[a['type']];fmt='<'+formats[a['componentType']]*n
    def decode(view,offset,count,fmt):
        v=doc['bufferViews'][view];offset+=v.get('byteOffset',0)
        stride=v.get('byteStride',struct.calcsize(fmt))
        return [list(struct.unpack_from(fmt,binary,offset+i*stride)) for i in range(count)]
    result=decode(a['bufferView'],a.get('byteOffset',0),a['count'],fmt) if 'bufferView' in a else [[0]*n for _ in range(a['count'])]
    if 'sparse' in a:
        sparse=a['sparse'];ix=sparse['indices'];vs=sparse['values']
        indices=decode(ix['bufferView'],ix.get('byteOffset',0),sparse['count'],'<'+formats[ix['componentType']])
        values=decode(vs['bufferView'],vs.get('byteOffset',0),sparse['count'],fmt)
        for i,v in zip(indices,values):result[i[0]]=v
    return result

def prepare(source,out):
    dest,binary=read(source/'night-striker.glb');src,raw=read(source/'universal-animation-library.glb')
    names={n.get('name'):i for i,n in enumerate(dest['nodes'])}
    srcnames={n.get('name'):i for i,n in enumerate(src['nodes'])}
    def store(values,kind='VEC3',integer=False):
        while len(binary)%4:binary.append(0)
        offset=len(binary);fmt='<'+('I' if integer else 'f')*len(values[0])
        for row in values:binary.extend(struct.pack(fmt,*row))
        view=len(dest.setdefault('bufferViews',[]));dest['bufferViews'].append({'buffer':0,'byteOffset':offset,'byteLength':len(binary)-offset})
        acc={'bufferView':view,'componentType':5125 if integer else 5126,'count':len(values),'type':kind}
        if kind=='VEC3':acc.update(min=[min(row[i] for row in values) for i in range(3)],max=[max(row[i] for row in values) for i in range(3)])
        idx=len(dest.setdefault('accessors',[]));dest['accessors'].append(acc);return idx
    def parents(doc):
        p={}
        for i,n in enumerate(doc['nodes']):
            for child in n.get('children',[]):p[child]=i
        return p
    sp,dp=parents(src),parents(dest)
    def worldrot(doc,p,local):
        cache={}
        def get(i):
            if i not in cache:cache[i]=mul(get(p[i]),local[i]) if i in p else local[i]
            return cache[i]
        return [get(i) for i in range(len(doc['nodes']))]
    sr=[n.get('rotation',[0,0,0,1]) for n in src['nodes']]
    dr=[n.get('rotation',[0,0,0,1]) for n in dest['nodes']]
    sw=worldrot(src,sp,sr);dw=worldrot(dest,dp,dr)
    # Preserve destination bone lengths; transfer animated rotation deltas in world space.
    selected={'Idle_Loop':'Idle','Walk_Loop':'Walk','Jog_Fwd_Loop':'Run','Sprint_Loop':'Sprint',
              'Pistol_Aim_Neutral':'Aim','Pistol_Shoot':'Fire','Pistol_Reload':'Reload',
              'Jump_Start':'Jump','Jump_Loop':'Fall','Jump_Land':'Land','Death01':'Death',
              'Hit_Chest':'Hit','Crouch_Idle_Loop':'Crouch','Crouch_Fwd_Loop':'CrouchWalk'}
    dest['animations']=[]
    clips={c['name']:c for c in src['animations']}
    work=[(clip,selected[clip['name']],False,0.,False) for clip in src['animations'] if clip['name'] in selected]
    work += [(clips['Walk_Loop'],'AimWalk',True,0.,False),
             (clips['Jog_Fwd_Loop'],'AimRun',True,0.,False),
             (clips['Walk_Loop'],'AimBack',True,0.,True),
             (clips['Walk_Loop'],'AimLeft',True,-math.pi/2,False),
             (clips['Walk_Loop'],'AimRight',True,math.pi/2,False),
             (clips['Walk_Loop'],'AimForwardLeft',True,-math.pi/4,False),
             (clips['Walk_Loop'],'AimForwardRight',True,math.pi/4,False),
             (clips['Walk_Loop'],'AimBackLeft',True,math.pi/4,True),
             (clips['Walk_Loop'],'AimBackRight',True,-math.pi/4,True)]
    for clip,output_name,aim,yaw,reverse in work:
        channels={}
        duration=0
        for c in clip['channels']:
            s=clip['samplers'][c['sampler']]
            if s.get('interpolation','LINEAR') not in ['LINEAR','STEP']:raise ValueError('unsupported interpolation')
            times=[v[0] for v in accessor(src,raw,s['input'])];values=accessor(src,raw,s['output'])
            channels[c['target']['node'],c['target']['path']]=(times,values,s.get('interpolation','LINEAR'))
            duration=max(duration,times[-1])
        if aim:
            # Offline upper-body mask: keep locomotion below pelvis while aiming the torso/arms.
            for c in clips['Pistol_Aim_Neutral']['channels']:
                if c['target']['node']>52 or c['target']['path']!='rotation':continue
                sampler=clips['Pistol_Aim_Neutral']['samplers'][c['sampler']]
                ts=[v[0] for v in accessor(src,raw,sampler['input'])]
                channels[c['target']['node'],'rotation']=(ts,accessor(src,raw,sampler['output']),'LINEAR')
        frames=max(2,int(math.ceil(duration*30))+1);times=[i*duration/(frames-1) for i in range(frames)]
        timeacc=store([[t] for t in times],'SCALAR');rotations={i:[] for i in range(65)}
        def sample(i,path,t,default):
            if (i,path) not in channels:return default
            ts,values,mode=channels[i,path]
            import bisect
            a=max(0,min(len(ts)-1,bisect.bisect_right(ts,t)-1));b=min(a+1,len(ts)-1)
            f=0 if a==b or mode=='STEP' else (t-ts[a])/(ts[b]-ts[a])
            return slerp(values[a],values[b],f) if path=='rotation' else [x+(y-x)*f for x,y in zip(values[a],values[b])]
        pelvis=[]
        for t in times:
            sample_time=duration-t if reverse else t
            pose=worldrot(src,sp,[sample(i,'rotation',sample_time,sr[i]) for i in range(len(sr))])
            target={i:mul(mul(pose[srcnames[dest['nodes'][i]['name']]],inv(sw[srcnames[dest['nodes'][i]['name']]])),dw[i]) for i in range(65)}
            if yaw:
                turn=[0,math.sin(yaw/2),0,math.cos(yaw/2)]
                for i in range(53,63):target[i]=mul(turn,target[i])
                hip_turn=[0,math.sin(yaw*.35/2),0,math.cos(yaw*.35/2)]
                target[63]=mul(hip_turn,target[63])
            for i in range(65):
                parent=dp.get(i);q=mul(inv(target[parent] if parent in target else dw[parent]),target[i]) if parent is not None else target[i]
                rotations[i].append(normalize(q))
            sourcebind=src['nodes'][srcnames['pelvis']].get('translation',[0,0,0]);motion=sample(srcnames['pelvis'],'translation',t,sourcebind)
            bind=dest['nodes'][names['pelvis']].get('translation',[0,0,0])
            # Source exporter is Z-up inside Armature. Keep vertical bob; strip planar root motion.
            pelvis.append([bind[0],bind[1],bind[2]+(motion[2]-sourcebind[2])*.99])
        anim={'name':output_name,'samplers':[],'channels':[]}
        for i,values in rotations.items():
            a=store(values,'VEC4');idx=len(anim['samplers']);anim['samplers'].append({'input':timeacc,'output':a,'interpolation':'LINEAR'})
            anim['channels'].append({'sampler':idx,'target':{'node':i,'path':'rotation'}})
        a=store(pelvis);idx=len(anim['samplers']);anim['samplers'].append({'input':timeacc,'output':a})
        anim['channels'].append({'sampler':idx,'target':{'node':names['pelvis'],'path':'translation'}})
        dest['animations'].append(anim)
    # Split the base body's existing indexed triangles into skin, sleeves/trousers and boots.
    mats=dest.setdefault('materials',[])
    def material(name,colour,metal=0):
        i=len(mats);mats.append({'name':name,'pbrMetallicRoughness':{'baseColorFactor':[*colour,1],'metallicFactor':metal,'roughnessFactor':.78}});return i
    cloth=material('FieldUniform',[.13,.18,.12]);boots=material('CombatBoots',[.045,.052,.042]);armor=material('PlateCarrier',[.09,.12,.075],.1)
    for node in dest['nodes']:
        if node.get('name')!='SuperHero_Male':continue
        mesh=dest['meshes'][node['mesh']];original=mesh['primitives'];mesh['primitives']=[]
        for prim in original:
            positions=accessor(dest,binary,prim['attributes']['POSITION']);indices=[v[0] for v in accessor(dest,binary,prim['indices'])]
            groups={prim.get('material',0):[],cloth:[],boots:[]}
            for k in range(0,len(indices),3):
                ids=indices[k:k+3];height=sum(positions[i][1] for i in ids)/3
                m=prim.get('material',0) if height>1.55 else boots if height<.2 else cloth
                groups[m].extend([[i] for i in ids])
            for m,idx in groups.items():
                if idx:p=copy.deepcopy(prim);p['indices']=store(idx,'SCALAR',True);p['material']=m;mesh['primitives'].append(p)
    # Bind-space rigid attachments: their node transform follows the skeleton, without duplicated skinning.
    # Compose bind positions and rotations for coordinates expressed in model space.
    wt={}
    def worldpos(i):
        if i not in wt:
            local=dest['nodes'][i].get('translation',[0,0,0]);parent=dp.get(i)
            wt[i]=[a+b for a,b in zip(worldpos(parent),rotate(dw[parent],local))] if parent is not None else local
        return wt[i]
    def box(name,bone,center,size,mat):
        j=names[bone];q=inv(dw[j]);origin=worldpos(j)
        pts=[]
        for z in [-1,1]:
            for y in [-1,1]:
                for x in [-1,1]:pts.append(rotate(q,[center[k]+[x,y,z][k]*size[k]*.5-origin[k] for k in range(3)]))
        faces=[(0,2,3,1),(4,5,7,6),(0,1,5,4),(2,6,7,3),(0,4,6,2),(1,3,7,5)]
        if name=='Helmet':
            pts=[];faces=[]
            for ring in range(6):
                angle=.06+(math.pi*.5-.06)*ring/5
                for side in range(16):
                    a=side*2*math.pi/16
                    point=[math.sin(angle)*math.cos(a)*.12,1.69+math.cos(angle)*.15,math.sin(angle)*math.sin(a)*.125]
                    pts.append(rotate(q,[point[k]-origin[k] for k in range(3)]))
            for ring in range(5):
                for side in range(16):
                    a=ring*16+side;b=ring*16+(side+1)%16;c=a+16;d=b+16
                    faces.extend([(a,b,c),(b,d,c)])
        vertices=[];normals=[];idx=[]
        for face in faces:
            a,b,c=[pts[i] for i in face[:3]];u=[b[i]-a[i] for i in range(3)];v=[c[i]-a[i] for i in range(3)]
            n=[u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]];l=math.sqrt(sum(x*x for x in n));n=[x/l for x in n]
            base=len(vertices);vertices.extend(pts[i] for i in face);normals.extend([n]*len(face));idx.extend([[base+i] for i in ([0,1,2] if len(face)==3 else [0,1,2,0,2,3])])
        mesh=len(dest['meshes']);dest['meshes'].append({'name':name,'primitives':[{'attributes':{'POSITION':store(vertices),'NORMAL':store(normals)},'indices':store(idx,'SCALAR',True),'material':mat}]})
        node=len(dest['nodes']);dest['nodes'].append({'name':name,'mesh':mesh});dest['nodes'][j].setdefault('children',[]).append(node)
    box('FrontArmor','spine_03',[0,1.30,.12],[.39,.34,.09],armor)
    box('BackArmor','spine_03',[0,1.30,-.11],[.43,.39,.11],armor)
    for x in [-.14,0,.14]:box('MagazinePouch','spine_03',[x,1.14,.21],[.115,.16,.07],armor)
    box('Belt','pelvis',[0,.94,0],[.42,.07,.25],boots)
    box('Helmet','Head',[0,1.76,.005],[.22,.12,.22],armor)
    box('Goggles','Head',[0,1.70,.102],[.17,.055,.035],boots)
    # Reuse the project's already-pinned Kenney weapon. Its mounting axis is explicit and named.
    weapon,wb=read(ROOT/'Assets/Source/TPS/Rifle.glb')
    while len(binary)%4:binary.append(0)
    offset=len(binary);binary.extend(wb)
    offsets={k:len(dest.setdefault(k,[])) for k in ['bufferViews','accessors','images','textures','samplers','materials','meshes','nodes']}
    for v in weapon.get('bufferViews',[]):
        v=copy.deepcopy(v);v['buffer']=0;v['byteOffset']=offset+v.get('byteOffset',0);dest['bufferViews'].append(v)
    for a in weapon.get('accessors',[]):
        a=copy.deepcopy(a)
        if 'bufferView' in a:a['bufferView']+=offsets['bufferViews']
        dest['accessors'].append(a)
    for im in weapon.get('images',[]):
        im=copy.deepcopy(im);im['bufferView']+=offsets['bufferViews'];dest['images'].append(im)
    dest['samplers'].extend(weapon.get('samplers',[]))
    for tex in weapon.get('textures',[]):
        tex=copy.deepcopy(tex);tex['source']+=offsets['images']
        if 'sampler' in tex:tex['sampler']+=offsets['samplers']
        dest['textures'].append(tex)
    for m in weapon.get('materials',[]):
        m=copy.deepcopy(m)
        def remap(v):
            if isinstance(v,dict):
                for k,x in v.items():
                    if k.endswith('Texture') and isinstance(x,dict):x['index']+=offsets['textures']
                    else:remap(x)
        remap(m)
        pbr=m.setdefault('pbrMetallicRoughness',{});pbr.pop('baseColorTexture',None)
        pbr.update(baseColorFactor=[.085,.10,.105,1],metallicFactor=.65,roughnessFactor=.42)
        dest['materials'].append(m)
    for m in weapon['meshes']:
        m=copy.deepcopy(m)
        for p in m['primitives']:
            p['attributes']={k:v+offsets['accessors'] for k,v in p['attributes'].items()}
            p['indices']+=offsets['accessors'];p['material']=p.get('material',0)+offsets['materials']
        dest['meshes'].append(m)
    for n in weapon['nodes']:
        n=copy.deepcopy(n);n['name']='Rifle_'+n.get('name','Node')
        if 'mesh' in n:n['mesh']+=offsets['meshes']
        if 'children' in n:n['children']=[i+offsets['nodes'] for i in n['children']]
        dest['nodes'].append(n)
    mount=len(dest['nodes']);dest['nodes'].append({'name':'WeaponSocket','translation':[0,-.07,-.03],
        'rotation':[-.5,.5,.5,.5],'scale':[.65]*3,'children':[i+offsets['nodes'] for i in weapon['scenes'][weapon.get('scene',0)]['nodes']]})
    dest['nodes'][names['hand_r']].setdefault('children',[]).append(mount)
    write(out,dest,binary)
    print('Prepared human:',out,'clips:',[x['name'] for x in dest['animations']])
if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser();parser.add_argument('--source',type=Path,default=ROOT/'Assets/Source/Landscape');parser.add_argument('--output',type=Path,default=ROOT/'Bin-Int/TPS/Human.glb')
    a=parser.parse_args();prepare(a.source,a.output)
