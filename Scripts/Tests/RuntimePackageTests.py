#!/usr/bin/env python3
"""Executable bootstrap regressions; fake package markers do not prove GPU/assets load."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]

class RuntimePackageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = os.environ.get('CXX', 'cl' if os.name == 'nt' else 'c++')
        if not shutil.which(compiler):
            raise unittest.SkipTest(f'{compiler} is unavailable; native runtime acceptance remains pending')
        cls.temp = tempfile.TemporaryDirectory(prefix='plagc-runtime-paths-')
        cls.addClassCleanup(cls.temp.cleanup)
        cls.root = Path(cls.temp.name)
        cls.package = cls.root / 'Package with spaces'
        cls.package.mkdir()
        cls.caller = cls.root / 'Unrelated caller'
        cls.caller.mkdir()
        cls.exe = cls.package / ('probe.exe' if os.name == 'nt' else 'probe')
        source = cls.root / 'probe.cpp'
        source.write_text('''#include <PlagC/RuntimePaths.h>
#include <iostream>
int main() {
    const auto before=std::filesystem::current_path();
    try { PlagC::EnterRuntimePackage(); std::cout << std::filesystem::current_path().generic_string(); }
    catch(const std::exception& e) {
        std::cerr << e.what();
        return std::filesystem::current_path()==before ? 2 : 3;
    }
}''')
        if Path(compiler).stem.lower() == 'cl':
            args = [compiler, '/nologo', '/std:c++20', '/EHsc',
                    '/I'+str(ROOT/'PlagC/include'), '/I'+str(ROOT/'Build/Pch'),
                    str(source), str(ROOT/'PlagC/src/RuntimePaths.cpp'), '/Fe:'+str(cls.exe)]
        else:
            args = [compiler, '-std=c++20', '-Wall', '-Wextra', '-Werror',
                    '-I'+str(ROOT/'PlagC/include'), '-I'+str(ROOT/'Build/Pch'),
                    str(source), str(ROOT/'PlagC/src/RuntimePaths.cpp'), '-o', str(cls.exe)]
        subprocess.run(args, cwd=cls.root, check=True)

    def test_launch_from_unrelated_directory(self):
        (self.package/'Assets').mkdir(exist_ok=True)
        media=self.package/'Vendor/ogre-next/Samples/Media/Hlms'
        media.mkdir(parents=True, exist_ok=True)
        result=subprocess.run([str(self.exe)], cwd=self.caller, text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(Path(result.stdout).resolve(), self.package.resolve())

    def test_missing_package_fails_without_changing_cwd(self):
        empty=self.root/'Missing resources'
        empty.mkdir(exist_ok=True)
        probe=empty/self.exe.name
        shutil.copy2(self.exe,probe)
        result=subprocess.run([str(probe)], cwd=self.caller, text=True, capture_output=True)
        self.assertEqual(result.returncode, 2)
        self.assertIn('Incomplete runtime package',result.stderr)
        self.assertIn(str(empty/'Assets'),result.stderr)

class PackageEvidenceTests(unittest.TestCase):
    def test_copied_package_requires_fresh_post_draw_evidence(self):
        import importlib.util
        from unittest.mock import patch
        spec=importlib.util.spec_from_file_location('package_check',ROOT/'Scripts/ValidateEditorPackages.py')
        check=importlib.util.module_from_spec(spec)
        spec.loader.exec_module(check)
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            source=root/'Release'
            (source/'Logs').mkdir(parents=True)
            (source/'Editor.exe').write_bytes(b'fixture')
            (source/'Logs'/'Ogre.log').write_text('stale evidence')
            def fake(args, cwd, **kwargs):
                package=Path(args[0]).parent
                self.assertNotEqual(package,source)
                self.assertNotEqual(package,cwd)
                self.assertFalse((package/'Logs').exists())
                (package/'Logs').mkdir()
                (package/'Logs/PlagC.log').write_text('Backend: Vulkan')
                (package/'Logs/Ogre.log').write_text('[PlagC UI] main-output draw completed: backend=Vulkan target=Window')
                return subprocess.CompletedProcess(args,0,b'',b'')
            with patch.object(check.subprocess,'run',side_effect=fake):
                check.run_case(source,'vulkan',root/'evidence',10)
            self.assertTrue((root/'evidence/Logs/Ogre.log').is_file())
            self.assertEqual((source/'Logs/Ogre.log').read_text(),'stale evidence')
        for engine,ogre in [
            ('Backend: OpenGL','[PlagC UI] main-output draw completed: backend=Vulkan target=Window'),
            ('Backend: Vulkan','[PlagC UI] explicit output=Window vertices=12'),
            ('Backend: Vulkan','[PlagC UI] main-output draw completed: backend=OpenGL target=Window'),
            ('Backend: Vulkan','[PlagC UI] ERROR: submitted UI was not presented')]:
            with self.assertRaises(ValueError):
                check.verify('vulkan',engine,ogre)

if __name__ == '__main__':
    unittest.main()
