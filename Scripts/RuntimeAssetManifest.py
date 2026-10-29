"""Shared preflight for engine-owned runtime data."""

from __future__ import annotations

from pathlib import Path

BASE = [
    "Assets/Textures/EnvironmentTest.dds",
    "Assets/Shaders/PlagCSky.material",
    "Assets/Shaders/PlagCSky_ps.glsl",
    "Assets/Shaders/PlagCSky_vs.glsl",
    "Assets/Shaders/PlagCToneMap.material",
    "Assets/Shaders/PlagCToneMap_ps.glsl",
    "Assets/Shaders/PlagCToneMap_vs.glsl",
    "Assets/Shaders/PlagCBloom_ps.glsl",
    "Assets/Compositors/PlagC.compositor",
]


def verify(root: Path, profile: str = "base", selected=None) -> None:
    """Verify only engine-owned resources required by the runtime.

    `profile` and `selected` are retained for command-line compatibility with older
    staging invocations; game/content validation belongs to the client project.
    """
    del profile, selected
    root = Path(root)
    missing = [name for name in BASE if not (root / name).is_file()]
    if missing:
        raise ValueError("Missing engine runtime files:\n  " + "\n  ".join(missing))
