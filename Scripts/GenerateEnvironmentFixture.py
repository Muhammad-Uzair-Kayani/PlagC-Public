"""Reproduce original DDS fixtures for Sandbox (Python 3, no packages).
Faces: +X red, -X cyan, +Y blue, -Y yellow, +Z green, -Z magenta.
Mip levels blend toward grey for deterministic rough-reflection testing;
these are test assets, not physically prefiltered production IBL maps.
"""
from pathlib import Path
import struct

root = Path(__file__).resolve().parents[1] / 'Assets/Textures'
colours = [(240, 45, 30), (30, 220, 220), (45, 80, 245),
           (240, 210, 35), (40, 225, 65), (220, 40, 220)]

def write(name, faces, size=16, mips=5):
    # Legacy DDS, RGBA8, all six cubemap faces; face-major mip chains.
    header = [124, 0x2100F, size, size, size * 4, 0, mips] + [0] * 11
    header += [32, 0x41, 0, 32, 0xFF, 0xFF00, 0xFF0000, 0xFF000000]
    header += [0x401008, 0xFE00, 0, 0, 0]
    data = bytearray(b'DDS ' + struct.pack('<31I', *header))
    for colour in faces:
        for mip in range(mips):
            fade = mip / max(1, mips - 1)
            rgb = [round(c * (1 - fade) + 128 * fade) for c in colour]
            data += bytes(rgb + [255]) * max(1, size >> mip) ** 2
    (root / name).write_bytes(data)

write('EnvironmentTest.dds', colours)
write('EnvironmentWarm.dds', [(230, 130, 30)] * 6)
write('EnvironmentNoMips.dds', colours, size=1, mips=1)
