"""Launch a built game using the same Python interpreter as this script."""
import argparse
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]

def locate_runtime(explicit=None):
    if explicit:
        runtime = Path(explicit).resolve()
        if not runtime.is_file():
            raise FileNotFoundError(f'Runtime not found: {runtime}')
        return runtime
    for config in ('Release', 'Debug'):
        candidates = []
        for name in ('GameRuntime.exe', 'GameRuntime'):
            candidates.extend((ROOT / 'build/premake').glob(f'*/bin/{config}/{name}'))
            candidates.extend((ROOT / 'build').glob(f'{config}/{name}'))
            candidates.extend((ROOT / 'build').glob(name))
        candidates = [p for p in candidates if p.is_file()]
        if candidates:
            return max(candidates, key=lambda p: p.stat().st_mtime)
    raise FileNotFoundError('Build GameRuntime first, or pass --runtime PATH.')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', help='Explicit path to GameRuntime executable')
    args, game_args = parser.parse_known_args()
    if sys.version_info < (3, 10):
        parser.error('Python 3.10 or newer is required')
    try:
        runtime = locate_runtime(args.runtime)
        command = [str(runtime), '--python', sys.executable,
                   '--policy-service', str(ROOT / 'python/service.py'),
                   '--model', str(ROOT / 'docs/samples/champion.json')]
        if '--headless' in game_args or '--server' in game_args:
            command += ['--controller', 'neural']
        command += game_args
        return subprocess.call(command, cwd=ROOT)
    except (OSError, ValueError) as exc:
        print(f'Cannot launch game: {exc}', file=sys.stderr)
        return 1

if __name__ == '__main__':
    raise SystemExit(main())
