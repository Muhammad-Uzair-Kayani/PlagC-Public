#!/usr/bin/env python3
"""Execute actual generated upload blocks with a recording buffer; no GPU required."""
from pathlib import Path
import os
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as folder:
    tmp = Path(folder)
    runner = tmp/'patch.lua'
    runner.write_text('local f=assert(io.open(arg[2],"rb"));local s=f:read("*a");f:close(); f=assert(io.open(arg[3],"wb")); f:write(dofile(arg[1])(s));f:close()')
    generated = tmp/'renderer.cpp'
    subprocess.run([os.environ.get('LUA','texlua'),str(runner),str(ROOT/'Build/Patches/OgreRendererPerformance.lua'),str(ROOT/'PlagC/src/Renderer/OgreRenderer.cpp'),str(generated)],check=True)
    source=generated.read_text()
    update=source[source.index('void Renderer::UpdateMesh('):source.index('TextureHandle Renderer::CreateTexture(')]
    stage=update[update.index('    if (backend_ == RendererBackend::Vulkan'):update.index('    mesh.value->_setBounds')]
    render=source[source.index('void Renderer::Render(double dt)'):]
    flush=render[render.index('    for (auto id : p.pendingMeshUploads)'):render.index('    if (!p.root->renderOneFrame())')]
    prelude=r'''
#include <cstring>
#include <span>
#include <vector>
#include <map>
#include <cstdint>
#include <stdexcept>
#include <iostream>
struct Vertex { float x; };
namespace Ogre { enum { VpNormal=0,UO_KEEP_PERSISTENT=1,BT_DYNAMIC_PERSISTENT=2 }; }
enum class RendererBackend { Vulkan,OpenGL };
struct Handle {std::uint64_t value;};
struct Buffer {
 int maps=0,unmaps=0,uploads=0;bool fail=false;std::vector<Vertex> data;
 int getBufferType(){return Ogre::BT_DYNAMIC_PERSISTENT;}
 void* map(int start,size_t count){if(fail)throw std::runtime_error("map failed");if(start)throw std::runtime_error("offset");++maps;data.resize(count);return data.data();}
 void unmap(int mode){if(mode!=Ogre::UO_KEEP_PERSISTENT)throw std::runtime_error("unmap");++unmaps;}
 void upload(const Vertex* p,int,size_t n){++uploads;data.assign(p,p+n);}
};
struct Vao {Buffer* b;std::vector<Buffer*> getVertexBuffers(){return {b};}};
struct Sub {std::vector<Vao*> mVao[1];};
struct Value {Sub sub;Sub* getSubMesh(int){return &sub;}};
struct Mesh {Value* value;std::vector<Vertex> pendingVertices;bool uploadQueued=false;};
struct State {std::map<std::uint64_t,Mesh> meshes;std::vector<std::uint64_t> pendingMeshUploads;};
void check(bool b){if(!b)throw std::runtime_error("regression");}
'''
    tests=r'''
int main(){
 Buffer b;Vao vao{&b};Value value{{{{&vao}}}};State p;p.meshes.emplace(1,Mesh{&value,{}});
 auto& mesh=p.meshes.at(1);Handle h{1};
 {std::vector<Vertex> v{{1},{2}};stage(RendererBackend::Vulkan,mesh,&b,v,&p,h);v[0].x=99;}
 check(mesh.pendingVertices[0].x==1&&b.maps==0);
 std::vector<Vertex> v{{3},{4}};
 stage(RendererBackend::Vulkan,mesh,&b,v,&p,h);
 check(p.pendingMeshUploads.size()==1);flush(p);
 check(b.maps==1&&b.unmaps==1&&b.data[0].x==3&&!mesh.uploadQueued&&p.pendingMeshUploads.empty());
 flush(p);check(b.maps==1);
 for(int f=0;f<12;++f){v[0].x=float(f);stage(RendererBackend::Vulkan,mesh,&b,v,&p,h);stage(RendererBackend::Vulkan,mesh,&b,v,&p,h);flush(p);check(b.maps==f+2&&b.data[0].x==f);}
 stage(RendererBackend::Vulkan,mesh,&b,v,&p,h);b.fail=true;bool failed=false;try{flush(p);}catch(const std::runtime_error&){failed=true;}
 check(failed&&!mesh.pendingVertices.empty()&&!p.pendingMeshUploads.empty());b.fail=false;flush(p);
 stage(RendererBackend::OpenGL,mesh,&b,v,&p,h);check(b.uploads==1&&p.pendingMeshUploads.empty());
 stage(RendererBackend::Vulkan,mesh,&b,v,&p,h);p.meshes.erase(1);int calls=b.maps;flush(p);check(b.maps==calls&&p.pendingMeshUploads.empty());
 std::cout<<"Vulkan upload ownership/coalescing/retry/destruction and GL dispatch passed\n";
}
'''
    cpp=tmp/'test.cpp';cpp.write_text(prelude+'\nvoid stage(RendererBackend backend_,Mesh& mesh,Buffer* buffer,std::span<const Vertex> vertices,State* impl_,Handle handle){\n'+stage+'}\nvoid flush(State& p){\n'+flush+'}\n'+tests)
    subprocess.run([os.environ.get('CXX','g++'),'-std=c++20','-O2','-Wall','-Wextra',str(cpp),'-o',str(tmp/'test')],check=True)
    subprocess.run([str(tmp/'test')],check=True)
