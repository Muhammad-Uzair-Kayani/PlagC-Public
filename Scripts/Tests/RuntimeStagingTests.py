"""Packaging must reject unvalidated content and repair asset-only rebuilds."""

import hashlib, json, os, subprocess, sys, tempfile, unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from RuntimeAssetManifest import BASE, RECEIPT, digest, verify
from StageRuntimeAssets import sync_tree

SCRIPT = Path(__file__).resolve().parents[1] / "StageRuntimeAssets.py"


class StagingTests(unittest.TestCase):
    def fixture(self, root):
        config = root / "Game/SurvivalGame/Config/worlds.json"
        config.parent.mkdir(parents=True, exist_ok=True)
        config.write_text("{}")
        license_file = root / "Vendor/glslang/LICENSE.txt"
        license_file.parent.mkdir(parents=True, exist_ok=True)
        license_file.write_text("compiler license fixture")
        for name in BASE:
            p = root / name
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(b"base")
        for name in ["Hlms", "models"]:
            p = root / "Vendor/ogre-next/Samples/Media" / name / "fixture"
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(b"media")
        directory = root / "Assets/Manifests"
        directory.mkdir()
        output = "Assets/Cooked/TPS/AAA/Camera.plasset"
        model = root / output
        model.parent.mkdir(parents=True, exist_ok=True)
        model.write_bytes(b"cooked")
        lock = directory / "AAAWorld.lock.json"
        lock.write_text(json.dumps({"assets": [{"slug": "camera", "output": output}]}))
        texture = root / "Assets/Textures/map.jpg"
        texture.write_bytes(b"image")
        (directory / "VisualTextures.lock.json").write_text(
            json.dumps({"files": [{"path": "map.jpg", "sha256": digest(texture)}]})
        )
        human = root / "Assets/Cooked/TPS/Human.plasset"
        human.write_bytes(b"human")
        landscape_lock = directory / "Landscape.lock.json"
        landscape_lock.write_text(json.dumps({"files": []}))
        (directory / "Landscape.LICENSES.txt").write_text("CC0")
        built = root / "Assets/Cooked/TPS/Landscape/build-receipt.json"
        built.parent.mkdir(parents=True)
        built.write_text(json.dumps({"lock_sha256": digest(landscape_lock), "human_sha256": digest(human)}))
        (root / RECEIPT).write_text(
            json.dumps(
                {
                    "lock_sha256": digest(lock),
                    "models": {"camera": {"output": output, "sha256": digest(model)}},
                }
            )
        )

    def stage(self, root, output):
        return subprocess.run(
            [sys.executable, str(SCRIPT), "--root", str(root), "--output", str(output)],
            capture_output=True,
            text=True,
        )

    def test_full_package_passes_and_does_not_copy_sources(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "repo"
            root.mkdir()
            self.fixture(root)
            p = root / "Assets/Source/large.bin"
            p.parent.mkdir()
            p.write_bytes(b"not runtime")
            out = Path(tmp) / "bin"
            result = self.stage(root, out)
            self.assertEqual(result.returncode, 0, result.stderr)
            verify(out)
            self.assertEqual((out / "Licenses/glslang-LICENSE.txt").read_text(), "compiler license fixture")
            self.assertTrue((out / "Game/SurvivalGame/Config/worlds.json").is_file())
            self.assertFalse((out / "Assets/Source").exists())

    def test_missing_required_model_fails_before_copy(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "repo"
            root.mkdir()
            self.fixture(root)
            (root / "Assets/Cooked/TPS/AAA/Camera.plasset").unlink()
            out = Path(tmp) / "bin"
            result = self.stage(root, out)
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse(out.exists())

    def test_corrupt_cooked_file_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.fixture(root)
            (root / "Assets/Cooked/TPS/AAA/Camera.plasset").write_bytes(b"broken")
            with self.assertRaisesRegex(ValueError, "not validated"):
                verify(root)

    def test_receipt_for_another_lock_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.fixture(root)
            p = root / "Assets/Manifests/AAAWorld.lock.json"
            p.write_text(p.read_text() + "\n")
            with self.assertRaisesRegex(ValueError, "different source lock"):
                verify(root)

    def test_changed_human_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.fixture(root)
            (root / "Assets/Cooked/TPS/Human.plasset").write_bytes(b"changed")
            with self.assertRaisesRegex(ValueError, "human changed"):
                verify(root)

    def test_missing_human_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.fixture(root)
            (root / "Assets/Cooked/TPS/Human.plasset").unlink()
            with self.assertRaisesRegex(ValueError, "Human/landscape"):
                verify(root)

    def test_overlap_is_rejected_without_deleting_source(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.fixture(root)
            result = self.stage(root, root)
            self.assertNotEqual(result.returncode, 0)
            self.assertTrue((root / BASE[0]).is_file())

    def test_same_size_and_timestamp_change_is_copied(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / "src"
            source.mkdir()
            p = source / "shader"
            p.write_bytes(b"old!")
            dest = Path(tmp) / "dst"
            sync_tree(source, dest)
            stamp = p.stat().st_mtime_ns
            p.write_bytes(b"new!")
            os.utime(p, ns=(stamp, stamp))
            sync_tree(source, dest)
            self.assertEqual((dest / "shader").read_bytes(), b"new!")

    def test_missing_staged_file_and_stale_file_are_repaired(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / "src"
            source.mkdir()
            (source / "shader").write_bytes(b"ok")
            dest = Path(tmp) / "dst"
            sync_tree(source, dest)
            (dest / "shader").unlink()
            (dest / "stale").write_bytes(b"old")
            sync_tree(source, dest)
            self.assertEqual((dest / "shader").read_bytes(), b"ok")
            self.assertFalse((dest / "stale").exists())


if __name__ == "__main__":
    unittest.main()
