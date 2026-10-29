#!/usr/bin/env python3
"""Exercise the actual Vulkan readback loop with recorded query results and frame fences."""
import os
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
s=(root/'PlagC/src/Renderer/GpuProfiler.cpp').read_text()
a=s.index('    std::uint64_t newestCompletedFrame = 0;')
code=s[a:s.index('    auto&slot=p.slots[p.write];',a)]
prelude=r'''
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <vector>
#include <set>
#include <cstring>
#include <iostream>
#include <stdexcept>
constexpr size_t GpuPassCount=3;
using VkResult=int;
constexpr int VK_SUCCESS=0,VK_NOT_READY=1,VK_QUERY_RESULT_64_BIT=1,VK_QUERY_RESULT_WITH_AVAILABILITY_BIT=2;
struct Timing{bool valid=false;double ms=0;};
struct Latest{bool supported=true,valid=false;unsigned outstandingFrames=0,latencyFrames=0;double totalMs=0;std::array<Timing,GpuPassCount> passes{};};
struct Vao{std::set<unsigned> completed;bool isFrameFinished(unsigned f){return completed.count(f)!=0;}};
struct Device{int mDevice=1;};
struct Impl {
 static constexpr unsigned RingSize=3,VkQueriesPerSlot=GpuPassCount*2;
 struct Query{bool begun=false,issued=false;};
 struct Slot{bool pending=false,started=false,resetIssued=false;unsigned gpuFrame=0;std::uint64_t frame=0;std::array<Query,GpuPassCount> passes{};};
 std::array<Slot,RingSize> slots{};Vao* vaoManager;Device* vulkanDevice;int queryPool=1;
 double timestampPeriodNs=1e6;std::uint64_t frame=20;Latest latest;
 std::array<std::vector<double>,GpuPassCount> samples;
};
std::array<std::array<std::array<std::uint64_t,2>,Impl::VkQueriesPerSlot>,Impl::RingSize> gpu{};
int calls=0,result=VK_NOT_READY;
VkResult vkGetQueryPoolResults(int,int,unsigned first,unsigned count,size_t bytes,void* data,size_t stride,int flags){
 if(count!=Impl::VkQueriesPerSlot||bytes!=sizeof(gpu[0])||stride!=sizeof(gpu[0][0])||flags!=(VK_QUERY_RESULT_64_BIT|VK_QUERY_RESULT_WITH_AVAILABILITY_BIT))throw std::runtime_error("unexpected query request");
 ++calls;std::memcpy(data,gpu[first/count].data(),bytes);return result;
}
void check(bool b){if(!b)throw std::runtime_error("readback regression");}
'''
tests=r'''
int main(){
 Vao vao;Device device;Impl p{};p.vaoManager=&vao;p.vulkanDevice=&device;
 auto& slot=p.slots[0];slot.pending=true;slot.gpuFrame=2;slot.frame=2;slot.passes[0].issued=true;
 gpu[0][0]={100,1};gpu[0][1]={110,1}; // stale available results from an earlier slot use
 poll(p);check(calls==0&&slot.pending&&!p.latest.valid);
 p.latest.outstandingFrames=0;vao.completed.insert(2);gpu[0][1][1]=0;
 poll(p);check(calls==1&&slot.pending&&!p.latest.valid); // incomplete issued query
 p.latest.outstandingFrames=0;gpu[0][1][1]=1;
 poll(p);check(calls==2&&!slot.pending&&p.latest.valid&&p.latest.totalMs==10&&p.samples[0].size()==1);
 // VK_NOT_READY from unissued query pairs must not suppress valid issued results.
 check(p.latest.supported&&p.latest.outstandingFrames==0);
 p.latest={};p.latest.supported=true;calls=0;
 for(unsigned i=0;i<3;++i){auto& x=p.slots[i];x.pending=true;x.frame=10-i;x.gpuFrame=10-i;x.passes[0].issued=true;vao.completed.insert(x.gpuFrame);gpu[i][0]={100,1};gpu[i][1]={110+i,1};}
 poll(p);check(calls==3&&p.latest.totalMs==10&&p.latest.latencyFrames==10&&p.samples[0].size()==4);
 p.slots[0].pending=true;p.slots[0].passes[0].issued=true;result=-4;
 poll(p);check(!p.latest.supported&&p.slots[0].pending);
 std::cout<<"Vulkan readback frame gating, batched availability, newest-result ordering and error tests passed\n";
}
'''
with tempfile.TemporaryDirectory() as folder:
 p=Path(folder);(p/'test.cpp').write_text(prelude+'\nvoid poll(Impl& p){\n'+code+'}\n'+tests)
 subprocess.run([os.environ.get('CXX','g++'),'-std=c++20','-O2','-Wall','-Wextra',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
