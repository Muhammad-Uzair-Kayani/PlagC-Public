#!/usr/bin/env python3
"""Run actual Debug/Dist Sandbox executables sequentially and summarize CSV samples."""
import argparse
import csv
import itertools
import json
import math
import platform
import subprocess
from pathlib import Path


def percentile(values, fraction):
    values = sorted(values)
    return values[min(len(values) - 1, int(fraction * (len(values) - 1)))]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--debug', required=True, type=Path)
    parser.add_argument('--dist', required=True, type=Path)
    parser.add_argument('--output', type=Path, default=Path('Logs/PhysicsMatrix'))
    parser.add_argument('--workers', nargs='+', type=int, default=[1, 2, 4, 6, 11])
    parser.add_argument('--scenes', nargs='+', type=int, default=list(range(1, 15)))
    parser.add_argument('--schedulers', nargs='+', choices=['native', 'plagc'], default=['native', 'plagc'])
    parser.add_argument('--warmup', type=int, default=180)
    parser.add_argument('--samples', type=int, default=600)
    parser.add_argument('--sweep', action='store_true', help='Expand count/triangle/query sweeps; potentially many hours')
    parser.add_argument('--dry-run', action='store_true')
    args = parser.parse_args()
    if any(w < 1 or w > 32 for w in args.workers) or any(s < 1 or s > 14 for s in args.scenes):
        parser.error('workers must be 1..32 and scenes 1..14')
    if args.warmup < 0 or args.samples < 1:
        parser.error('warmup >= 0 and samples >= 1 required')
    executables = [('Debug', args.debug.resolve()), ('Dist', args.dist.resolve())]
    if not args.dry_run:
        for _, exe in executables:
            if not exe.is_file():
                parser.error(f'executable missing: {exe}')
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / 'machine.json').write_text(json.dumps({'platform': platform.platform(), 'processor': platform.processor(), 'command': vars(args)}, default=str, indent=2))
    summaries = []
    for (build, exe), workers, scheduler, scene in itertools.product(executables, args.workers, args.schedulers, args.scenes):
        # Representative workload; variants distinguish CCD, island topology, sleeping,
        # terrain representation, sensor count and distributed/connected ragdolls.
        bodies = {1: 30, 2: 60, 4: 2000, 5: 5000, 6: 10000, 10: 32, 12: 5000, 14: 3684}.get(scene, 1000)
        variants = [0, 1] if scene in [3, 5, 6, 7, 9, 11, 12, 14] else [0]
        counts = [bodies]
        triangles = [100000]
        queries = [5000]
        if args.sweep:
            if scene in [3, 4, 5, 11, 12]: counts = [100, 500, 1000, 2000, 5000]
            if scene == 7: counts, triangles = [100, 500, 1000], [10000, 50000, 100000, 500000]
            if scene == 8: queries = [100, 500, 1000, 5000, 10000]
            if scene == 9: counts = [100, 1000]
        for variant, count, tris, query in itertools.product(variants, counts, triangles, queries):
            name = f'{build}-s{scene}-v{variant}-{scheduler}-w{workers}-b{count}-t{tris}-q{query}'
            path = (args.output / (name + '.csv')).resolve()
            command = [str(exe), '--physics-benchmark', '--physics-scene', str(scene), '--physics-bodies', str(count), '--physics-workers', str(workers), '--workers', str(workers if scheduler == 'plagc' else 1), '--physics-scheduler', scheduler, '--physics-triangles', str(tris), '--physics-queries', str(query), '--physics-seed', '42', '--physics-warmup', str(args.warmup), '--physics-samples', str(args.samples), '--physics-csv', str(path)]
            if variant: command += ['--physics-individual' if scene == 12 else '--physics-variant']
            print(subprocess.list2cmdline(command), flush=True)
            if args.dry_run: continue
            with (args.output / (name + '.log')).open('w') as log:
                subprocess.run(command, cwd=Path(__file__).resolve().parents[1], stdout=log, stderr=subprocess.STDOUT, check=True)
            with path.open(newline='') as f: rows = list(csv.DictReader(f))
            if len(rows) != args.samples: raise RuntimeError(f'incomplete benchmark: {path}')
            result = {'build': build, 'scene': scene, 'variant': variant, 'scheduler': scheduler, 'workers': workers, 'requested_bodies': count, 'triangles': tris, 'queries': query}
            for field in ['frame_ms', 'preparation_ms', 'physics_ms', 'input_ms', 'output_ms', 'sync_ms', 'query_ms', 'stream_ms', 'lod_ms', 'active', 'sleeping']:
                values = [float(row[field]) for row in rows]
                if not all(math.isfinite(v) for v in values): raise RuntimeError(f'non-finite {field}: {path}')
                result[field + '_median'] = percentile(values, .5)
                result[field + '_p95'] = percentile(values, .95)
            summaries.append(result)
            with (args.output / 'summary.csv').open('w', newline='') as f:
                writer = csv.DictWriter(f, fieldnames=list(result));writer.writeheader();writer.writerows(summaries)


if __name__ == '__main__':
    main()
