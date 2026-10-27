#!/usr/bin/env python3
"""Prove that bloom changes pixels outside the isolated emissive object, with no image packages."""
from pathlib import Path

def ppm(path):
    data=Path(path).read_bytes()
    # Ogre's P6 writer emits the magic, dimensions and max value as three ASCII lines.
    magic,dimensions,maximum,pixels=data.split(b'\n',3)
    if magic!=b'P6' or maximum!=b'255':raise ValueError('Unexpected PPM format')
    width,height=map(int,dimensions.split())
    if len(pixels)!=width*height*3:raise ValueError('Truncated PPM')
    return width,height,pixels

def main():
    width,height,on=ppm('Logs/Bloom-on.ppm');w,h,off=ppm('Logs/Bloom-off.ppm')
    assert (width,height)==(w,h)==(256,256)
    halo=[]
    for y in range(height):
        for x in range(width):
            if abs(x-128)<=12 and abs(y-128)<=12:continue
            i=(y*width+x)*3
            halo.extend(max(0,on[i+c]-off[i+c]) for c in range(3))
    assert max(halo)>5 and sum(halo)>1000,'Bloom did not spread beyond emissive geometry'
    assert ppm('Logs/Bloom-resize.ppm')[:2]==(320,180)
    print('[PASS] bloom produces an independent halo and resizes correctly')
if __name__=='__main__':main()
