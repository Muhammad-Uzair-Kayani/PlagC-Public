"""Run copied native Release/Dist packages outside the repository; keep per-run evidence."""
import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


def verify(backend, engine_log, ogre_log):
    if not re.search(r'\bBackend: '+re.escape(backend)+r'\b', engine_log, re.I):
        raise ValueError('Engine did not confirm the requested backend')
    if '[PlagC UI] ERROR:' in ogre_log:
        raise ValueError('UI backend reported a presentation error')
    if not re.search(r'\[PlagC UI\] main-output draw completed: backend='+
                     re.escape(backend)+r' target=\S+', ogre_log, re.I):
        raise ValueError('No completed UI draw on the requested main output')


def run_case(source, backend, evidence, timeout):
    evidence.mkdir(parents=True)
    with tempfile.TemporaryDirectory(prefix='plagc-package-') as tmp:
        package = Path(tmp)/'Standalone Editor'
        # Omit generated state, particularly old logs which could falsely pass a run.
        def omit(folder, names):
            return set(names) & {'Logs', 'Saved'} if Path(folder)==source else set()
        shutil.copytree(source, package, ignore=omit)
        caller = Path(tmp)/'Unrelated working directory'
        caller.mkdir()
        try:
            result = subprocess.run([str(package/'Editor.exe'), '--renderer', backend,
                                     '--frames', '24', '--workers', '1', '--no-audio'],
                                    cwd=caller, timeout=timeout, capture_output=True)
            (evidence/'stdout.txt').write_bytes(result.stdout)
            (evidence/'stderr.txt').write_bytes(result.stderr)
            if result.returncode:
                raise ValueError(f'Editor exit code {result.returncode}')
            logs = package/'Logs'
            verify(backend, (logs/'PlagC.log').read_text(errors='replace'),
                   (logs/'Ogre.log').read_text(errors='replace'))
        finally:
            if (package/'Logs').is_dir():
                shutil.copytree(package/'Logs', evidence/'Logs')


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bin-root', type=Path, default=root/'Bin/windows-x86_64')
    parser.add_argument('--reports', type=Path, default=root/'Build/ValidationLogs')
    args = parser.parse_args()
    args.reports.mkdir(parents=True, exist_ok=True)
    report = Path(tempfile.mkdtemp(prefix='editor-packages-', dir=args.reports))
    results = {}
    for config in ('Release','Dist'):
        for backend in ('opengl','vulkan'):
            case = config+'-'+backend
            try:
                run_case((args.bin_root/config).resolve(), backend, report/case, 120)
                results[case]='PASS'
            except (OSError, ValueError, subprocess.SubprocessError) as e:
                results[case]='FAIL: '+str(e)
            print(case, results[case], flush=True)
    (report/'results.json').write_text(json.dumps(results,indent=2))
    print('Evidence:',report)
    return 0 if all(x=='PASS' for x in results.values()) else 1


if __name__=='__main__':
    raise SystemExit(main())
