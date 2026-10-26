"""Offline regressions for source package integrity and dependency layout."""

import importlib.util, json, sys, tempfile, unittest, hashlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import FetchAAAWorld as fetch


class PackageTests(unittest.TestCase):
    def fixture(self, root):
        model = json.dumps(
            {
                "buffers": [{"uri": "camera.bin"}],
                "images": [{"uri": "textures/camera.jpg"}],
            }
        ).encode()
        files = {
            "camera.gltf": model,
            "camera.bin": b"geometry",
            "textures/camera.jpg": b"image",
        }
        includes = {
            name: {
                "url": "https://example.org/" + Path(name).name,
                "size": len(data),
                "md5": hashlib.md5(data).hexdigest(),
            }
            for name, data in files.items()
        }
        primary = includes.pop("camera.gltf")
        primary["include"] = includes
        asset = fetch.package(
            "camera",
            "Assets/Cooked/Camera.plasset",
            {"gltf": {"1k": {"gltf": primary}}},
            "1k",
        )
        for name, data in files.items():
            path = root / "camera" / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        return asset

    def test_preserves_relative_include_paths(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            asset = self.fixture(root)
            primary = fetch.fetch_asset(asset, root, offline=True)
            self.assertTrue((primary.parent / "textures/camera.jpg").is_file())

    def test_flattened_package_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            asset = self.fixture(root)
            (root / "camera/textures/camera.jpg").rename(root / "camera/camera.jpg")
            with self.assertRaisesRegex(ValueError, "missing/corrupt"):
                fetch.fetch_asset(asset, root, True)

    def test_corrupted_cache_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            asset = self.fixture(root)
            (root / "camera/camera.bin").write_bytes(b"corrupt!")
            with self.assertRaisesRegex(ValueError, "missing/corrupt"):
                fetch.fetch_asset(asset, root, True)

    def test_missing_lock_dependency_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            asset = self.fixture(root)
            asset["files"] = [
                f for f in asset["files"] if not f["path"].startswith("textures/")
            ]
            with self.assertRaisesRegex(ValueError, "unlocked glTF"):
                fetch.fetch_asset(asset, root, True)

    def test_traversal_is_rejected(self):
        for path in [
            "../outside.bin",
            "/absolute",
            "textures/../../outside",
            "C:/outside",
            "textures\\outside",
        ]:
            with self.subTest(path=path), self.assertRaises(ValueError):
                fetch.safe_relative(path)

    def test_valid_cache_never_calls_network(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            asset = self.fixture(root)
            original = fetch.request
            fetch.request = lambda _: self.fail("network called for valid pinned cache")
            try:
                fetch.fetch_asset(asset, root)
            finally:
                fetch.request = original


if __name__ == "__main__":
    unittest.main()
