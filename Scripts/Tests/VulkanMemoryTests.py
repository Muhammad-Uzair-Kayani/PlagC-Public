#!/usr/bin/env python3
"""Exercise the patched Ogre selection code without requiring a physical GPU.

Usage: python Scripts/Tests/VulkanMemoryTests.py [--ogre PATH] [--lua texlua]
Compiles the actual generated selection code with fake Vulkan memory properties.
"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--ogre', type=Path, default=ROOT / 'Vendor/ogre-next')
p.add_argument('--lua', default='texlua')
a = p.parse_args()
source = a.ogre / 'RenderSystems/Vulkan/src/Vao/OgreVulkanVaoManager.cpp'

with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    runner = work / 'patch.lua'
    runner.write_text(r'''local patch = dofile(arg[1])
local f = assert(io.open(arg[2], "rb")); local original = f:read("*a"); f:close()
original = original:gsub("\r\n", "\n")
local result = patch(original)
assert(result == patch(original:gsub("\n", "\r\n")), "CRLF checkout must produce identical source")
assert(result == patch(original), "generation must be deterministic")
assert(not pcall(patch, result), "changed upstream source must fail closed")
f = assert(io.open(arg[3], "wb")); f:write(result); f:close()
''')
    generated = work / 'OgreVulkanVaoManager.cpp'
    subprocess.run([a.lua, str(runner), str(ROOT / 'Build/Patches/VulkanMemory.lua'),
                    str(source), str(generated)], check=True)
    patched = generated.read_text()

    def harness(code):
        # Use the actual preference construction and allocation selection, rather
        # than a Python reimplementation that could repeat the same mistake.
        scoring = code[code.index('    static int calculateMemoryTypeScore('):
                       code.index('    uint32 VulkanVaoManager::determineSupportedMemoryTypes(')]
        setup = code[code.index('    void VulkanVaoManager::determineBestMemoryTypes()'):
                     code.index('    void VulkanVaoManager::allocateVbo(')]
        begin = code.index('            const VkMemoryHeap *memHeaps', code.index('    void VulkanVaoManager::allocateVbo('))
        selection = code[begin:code.index('            const size_t usablePoolSize = poolSize;', begin)]
        return PRELUDE + scoring + setup + '''
Choice VulkanVaoManager::choose(size_t sizeBytes, VboFlag vboFlag, uint32 textureMemTypeBits) {
''' + selection + '''
return {chosenMemoryTypeIdx, poolSize, bIsTextureOnly};
}
''' + CASES

    build_dir = work
    generated_source = patched
    original_source = source.read_text()
    def run_tests():
        compiler = os.environ.get('CXX', 'g++')
        for name, code, should_pass in [('patched', generated_source, True),
                                        ('upstream', original_source, False)]:
            cpp = build_dir / (name + '.cpp')
            exe = build_dir / name
            cpp.write_text(harness(code))
            subprocess.run([compiler, '-std=c++17', '-O2', '-Wall', '-Wextra',
                            str(cpp), '-o', str(exe)], check=True)
            for case in ['buffers', 'textures']:
                result = subprocess.run([str(exe), case], capture_output=True, text=True)
                print(name + '/' + case + ': ' + result.stdout.strip() + result.stderr.strip())
                assert (result.returncode == 0) == should_pass, name + '/' + case
        print('Vulkan memory regressions passed; unpatched allocator reproduces failure.')

    PRELUDE = r'''
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
using uint32 = uint32_t;
using VkDeviceSize = uint64_t;
using VkBufferUsageFlags = uint32_t;
constexpr uint32 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT=1, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT=2,
 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT=4, VK_MEMORY_PROPERTY_HOST_CACHED_BIT=8,
 VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD=256;
constexpr uint32 VK_BUFFER_USAGE_TRANSFER_SRC_BIT=1, VK_BUFFER_USAGE_TRANSFER_DST_BIT=2,
 VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT=4, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT=8,
 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT=16, VK_BUFFER_USAGE_INDEX_BUFFER_BIT=32,
 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT=64, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT=128;
#define OGRE_ASSERT_LOW(x) do { if(!(x)) throw std::runtime_error("assertion: " #x); } while(0)
#define OGRE_EXCEPT(type, message, where) throw std::runtime_error(message)
constexpr int LML_CRITICAL=1;
struct LogManager { static LogManager& getSingleton(){static LogManager l; return l;}
 void logMessage(const std::string&, int=0){} };
struct StringConverter { template<class T> static std::string toString(T v){return std::to_string(v);} };
template<class T> using FastArray=std::vector<T>;
struct VkMemoryHeap { uint64_t size=0; };
struct VkMemoryType { uint32 propertyFlags=0, heapIndex=0; };
struct VkPhysicalDeviceMemoryProperties {
 uint32 memoryTypeCount=0, memoryHeapCount=0;
 VkMemoryHeap memoryHeaps[8]; VkMemoryType memoryTypes[32];
};
struct Device { VkPhysicalDeviceMemoryProperties mDeviceMemoryProperties;
 struct { struct { uint64_t bufferImageGranularity=256; } limits; } mDeviceProperties; };
struct Choice { uint32 type; size_t pool; bool textureOnly; };
struct VulkanVaoManager {
 enum VboFlag {CPU_INACCESSIBLE, CPU_WRITE_PERSISTENT, CPU_WRITE_PERSISTENT_COHERENT,
               CPU_READ_WRITE, TEXTURES_OPTIMAL, MAX_VBO_FLAG};
 Device device; Device* mDevice=&device;
 FastArray<uint32> mBestVkMemoryTypeIndex[MAX_VBO_FLAG];
 uint32 mMemoryTypesInUse[MAX_VBO_FLAG]={}; uint32 supported=15;
 uint64_t mUsedHeapMemory[8]={}; size_t mDefaultPoolSize[MAX_VBO_FLAG]={64,64,64,64,64};
 bool mSupportsNonCoherentMemory=false, mSupportsCoherentMemory=false,
      mPreferCoherentMemory=false, mReadMemoryIsCoherent=false;
 void addMemoryType(VboFlag, const VkPhysicalDeviceMemoryProperties&, uint32);
 uint32 determineSupportedMemoryTypes(VkBufferUsageFlags) const {return supported;}
 void determineBestMemoryTypes();
 Choice choose(size_t, VboFlag, uint32);
};
'''
    CASES = r'''
void require(bool b, const char* what){if(!b) throw std::runtime_error(what);}
void initialize(VulkanVaoManager& v) {
 auto& p=v.device.mDeviceMemoryProperties;
 p.memoryHeapCount=2; p.memoryTypeCount=4;
 p.memoryHeaps[0].size=256; p.memoryHeaps[1].size=4096;
 p.memoryTypes[0]={1,0}; p.memoryTypes[1]={7,0};
 p.memoryTypes[2]={6,1}; p.memoryTypes[3]={14,1};
 v.determineBestMemoryTypes();
}
int main(int argc, char** argv){try{
 VulkanVaoManager v; initialize(v);
 auto gpu=VulkanVaoManager::CPU_INACCESSIBLE;
 auto textures=VulkanVaoManager::TEXTURES_OPTIMAL;
 if(argc>1 && std::string(argv[1])=="textures") {
 auto& p=v.device.mDeviceMemoryProperties;
 p.memoryHeapCount=3; p.memoryTypeCount=5;
 p.memoryHeaps[2].size=128; p.memoryTypes[4]={1,2};
 auto c=v.choose(30,textures,1u<<4);
 require(c.type==4 && c.textureOnly,"texture fallback failed on unused compatible heap");
 std::cout<<"texture fallback passed\n"; return 0;
 }
 auto c=v.choose(30,gpu,v.mMemoryTypesInUse[gpu]);
 require(c.type==0,"local memory must remain preferred");
 v.mUsedHeapMemory[0]=256;
 c=v.choose(30,gpu,v.mMemoryTypesInUse[gpu]);
 require(c.type==2 && c.pool==64,"full local heap must fall back to compatible shared heap");
 // Texture-only memory type omitted by the per-heap preference list, on an unused heap.
 auto& p=v.device.mDeviceMemoryProperties;
 p.memoryHeapCount=3; p.memoryTypeCount=5;
 p.memoryHeaps[2].size=128; p.memoryTypes[4]={1,2};
 c=v.choose(30,textures,1u<<4);
 require(c.type==4 && c.textureOnly,"texture fallback must use requested size, not SIZE_MAX");
 v.mUsedHeapMemory[2]=98;
 c=v.choose(30,textures,1u<<4);
 require(c.pool==30,"exact-fit texture fallback");
 v.mUsedHeapMemory[1]=4066;
 c=v.choose(30,gpu,v.mMemoryTypesInUse[gpu]);
 require(c.pool==30,"exact-fit shared buffer allocation");
 v.mUsedHeapMemory[1]=4097;
 bool rejected=false; try{v.choose(30,gpu,v.mMemoryTypesInUse[gpu]);}
 catch(const std::runtime_error&){rejected=true;}
 require(rejected,"overcommitted accounting must not underflow into free space");
 rejected=false; try{v.choose(30,textures,1u<<5);}
 catch(const std::runtime_error&){rejected=true;}
 require(rejected,"must reject incompatible memory types");
 VulkanVaoManager restricted; restricted.supported=3; initialize(restricted);
 require((restricted.mMemoryTypesInUse[gpu]&12)==0,"unsupported shared types must stay excluded");
 // Host-mapped classes retain their original visibility/coherency restrictions.
 for(auto t: v.mBestVkMemoryTypeIndex[VulkanVaoManager::CPU_WRITE_PERSISTENT_COHERENT])
 require((p.memoryTypes[t].propertyFlags&6)==6,"coherent buffers require host-visible coherent memory");
 std::cout<<"all selection cases passed\n"; return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
'''
    run_tests()
