#!/usr/bin/env python3
"""Static regression checks for the CPU skinning hot path.

These checks keep the per-frame AssetScene path allocation-free without needing a GPU/runtime.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
header = (ROOT / 'PlagC/include/PlagC/Animation/Animation.h').read_text()
animator = (ROOT / 'PlagC/src/Animation/OzzAnimator.cpp').read_text()
scene = (ROOT / 'PlagC/src/Assets/AssetScene.cpp').read_text()

assert 'std::span<const Vertex> DeformVertices(unsigned mesh) const;' in header
assert 'std::vector<Matrix4> skinMatrices;' in animator
assert 'std::vector<std::vector<Vertex>> deformedVertices;' in animator
assert 'std::span<const Vertex> Animator::DeformVertices(unsigned mesh) const' in animator
assert 'std::vector<Matrix4> skin(p.pose.size())' not in animator
assert 'auto data=instance.animator->Deform(i)' not in scene
assert 'auto vertices=instance.animator->DeformVertices(i)' in scene
assert 'UpdateMesh(instance.deformed[i],vertices)' in scene

print('Animation performance hot-path checks passed.')
