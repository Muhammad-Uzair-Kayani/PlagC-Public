#!/usr/bin/env python3
"""Original analytic dusk sky, RGBA8 DDS cubemap with box-filtered mipmaps.
This is a lightweight environment baseline, not a GGX-prefiltered HDRI.
"""
from pathlib import Path
import struct,math
size=64;mips=7
header=[124,0x2100F,size,size,size*4,0,mips]+[0]*11+[32,0x41,0,32,0xff,0xff00,0xff0000,0xff000000]+[0x401008,0xfe00,0,0,0]
data=bytearray(b'DDS '+struct.pack('<31I',*header))
for face in range(6):
    pixels=[]
    for y in range(size):
        for x in range(size):
            u=2*(x+.5)/size-1;v=2*(y+.5)/size-1
            direction=[(1,-v,-u),(-1,-v,u),(u,1,v),(u,-1,-v),(u,-v,1),(-u,-v,-1)][face]
            length=math.sqrt(sum(t*t for t in direction));height=direction[1]/length
            t=max(0,min(1,height));horizon=(166,159,143);zenith=(66,100,139)
            rgb=[round(horizon[i]*(1-t)+zenith[i]*t) for i in range(3)] if height>=0 else [round(c*(1+height*.6)) for c in (115,117,107)]
            pixels.append((*rgb,255))
    n=size
    for mip in range(mips):
        data.extend(bytes(v for p in pixels for v in p))
        if n==1:break
        reduced=[]
        for y in range(n//2):
            for x in range(n//2):
                samples=[pixels[(2*y+dy)*n+2*x+dx] for dy in range(2) for dx in range(2)]
                reduced.append(tuple(round(sum(p[c] for p in samples)/4) for c in range(4)))
        pixels=reduced;n//=2
out=Path(__file__).resolve().parents[1]/'Assets/Textures/PlagCEnvironment.dds'
out.write_bytes(data)
print(out)
