#!/usr/bin/env python3
"""Run repeatable integrated-world captures; execute from the repository root."""
import argparse
from pathlib import Path
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--exe', type=Path, required=True)
p.add_argument('--seconds', type=int, default=30)
p.add_argument('--output', type=Path, default=Path('Logs/StressMatrix'))
p.add_argument('--workers', type=int, nargs='+', default=[1, 2, 4, 6, 11])
p.add_argument('--entities', type=int, nargs='+', default=[1000, 2500, 5000, 10000])
p.add_argument('--schedulers', nargs='+', choices=['native', 'plagc'], default=['native', 'plagc'])
a = p.parse_args()
if not a.exe.is_file():
    p.error('executable does not exist')
a.output.mkdir(parents=True, exist_ok=True)
for scheduler in a.schedulers:
    for workers in a.workers:
        for entities in a.entities:
            stem = f'{scheduler}-{workers}w-{entities}'
            command = [str(a.exe.resolve()), '--benchmark', '--no-progressive', '--entities', str(entities),
                       '--seconds', str(a.seconds), '--physics-scheduler', scheduler,
                       '--workers', str(workers if scheduler == 'plagc' else 1),
                       '--physics-workers', str(workers), '--csv', str(a.output / (stem + '.csv'))]
            print('Running', stem, flush=True)
            with (a.output / (stem + '.log')).open('w') as log:
                subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
