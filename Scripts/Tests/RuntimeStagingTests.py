"Tests for engine-only runtime staging."

import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from RuntimeAssetManifest import BASE, verify
from StageRuntimeAssets import sync_tree

SCRIPT = Path(__file__).resolve().parents[1] / "StageRuntimeAssets.py"


class StagingTests(unittest.TestCase):
    def fixture(self, root: Path) -> None:
        license_file = root / "Vendor/glslang/LICENSE.txt"
        license_file.parent.mkdir(parents=True, exist_ok=True)
        license_file.write_text("compiler license fixture")

        for name in BASE:
            path = root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"engine-runtime")

        for name in ["Hlms", "models"]:
            path = root / "Vendor/ogre-next/Samples/Media" / name / "fixture"
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"media")

    def stage(self, root: Path, output: Path):
        return subprocess.run(
            [
                sys.executable,
                str(SCRIPT),
                "--root",
                str(root),
                "--output",
                str(output),
            ],
            capture_output=True,
            text=True,
        )

    def test_engine_package_passes_without_game_content(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "repo"
            root.mkdir()
            self.fixture(root)

            source = root / "Assets/Source/not-runtime.bin"
            source.parent.mkdir(parents=True)
            source.write_bytes(b"source")

            cooked_game = root / "Assets/Cooked/TPS/unused.plasset"
            cooked_game.parent.mkdir(parents=True)
            cooked_game.write_bytes(b"game")

            output = Path(temporary) / "bin"
            result = self.stage(root, output)
            self.assertEqual(result.returncode, 0, result.stderr)
            verify(output)
            self.assertEqual(
                (output / "Licenses/glslang-LICENSE.txt").read_text(),
                "compiler license fixture",
            )
            self.assertFalse((output / "Assets/Source").exists())
            self.assertFalse((output / "Assets/Cooked").exists())

    def test_missing_engine_runtime_file_fails_before_copy(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "repo"
            root.mkdir()
            self.fixture(root)
            (root / BASE[0]).unlink()

            output = Path(temporary) / "bin"
            result = self.stage(root, output)
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse(output.exists())

    def test_overlap_is_rejected_without_deleting_source(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.fixture(root)
            result = self.stage(root, root)
            self.assertNotEqual(result.returncode, 0)
            self.assertTrue((root / BASE[0]).is_file())

    def test_same_size_and_timestamp_change_is_copied(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "src"
            source.mkdir()
            path = source / "shader"
            path.write_bytes(b"old!")

            destination = Path(temporary) / "dst"
            sync_tree(source, destination)
            stamp = path.stat().st_mtime_ns
            path.write_bytes(b"new!")
            os.utime(path, ns=(stamp, stamp))
            sync_tree(source, destination)

            self.assertEqual((destination / "shader").read_bytes(), b"new!")

    def test_missing_and_stale_staged_files_are_repaired(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "src"
            source.mkdir()
            (source / "shader").write_bytes(b"ok")

            destination = Path(temporary) / "dst"
            sync_tree(source, destination)
            (destination / "shader").unlink()
            (destination / "stale").write_bytes(b"old")
            sync_tree(source, destination)

            self.assertEqual((destination / "shader").read_bytes(), b"ok")
            self.assertFalse((destination / "stale").exists())


if __name__ == "__main__":
    unittest.main()
