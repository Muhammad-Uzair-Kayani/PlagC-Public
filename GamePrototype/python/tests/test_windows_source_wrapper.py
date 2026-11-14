"""Validate the delivered Godot source wrapper on the existing Windows CI job."""
import importlib.util
import os
from pathlib import Path
import sys
import tempfile
import unittest
import zipfile

_SOURCE_COPY = None


def load_tests(loader, tests, pattern):
    global _SOURCE_COPY
    if os.name != 'nt':
        return tests
    repository = Path(__file__).resolve().parents[3]
    archive = repository / 'Deliverables/LastLight-Godot-Source.zip'
    _SOURCE_COPY = tempfile.TemporaryDirectory(prefix='Last Light Delivered Source ')
    with zipfile.ZipFile(archive) as z:
        z.extractall(_SOURCE_COPY.name)
    source = Path(_SOURCE_COPY.name) / 'LastLightGodot'
    sys.path.insert(0, str(source / 'scripts'))
    spec = importlib.util.spec_from_file_location('delivered_windows_native_tests', source / 'tests/test_windows_native.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    tests.addTests(loader.loadTestsFromTestCase(module.WindowsNativeTests))
    tests.addTests(loader.loadTestsFromTestCase(FullDeliveredGodotBuild))
    return tests


@unittest.skipUnless(os.name == 'nt', 'requires native Windows')
class FullDeliveredGodotBuild(unittest.TestCase):
    def test_complete_delivered_godot_build_and_gameplay(self):
        import subprocess
        source = Path(_SOURCE_COPY.name) / 'LastLightGodot'
        result = subprocess.run([sys.executable, str(source / 'tests/windows_full_build.py')],
                                cwd=source, text=True, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, timeout=6900)
        print(result.stdout, flush=True)
        self.assertEqual(result.returncode, 0, result.stdout)
