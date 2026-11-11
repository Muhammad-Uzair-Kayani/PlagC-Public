"""Seeded genetic training and held-out evaluation using the authoritative C++ runtime."""
import argparse
import csv
import json
import os
import random
import statistics
import subprocess
import sys
import time
from pathlib import Path
from neural import Policy, GENES

ROOT = Path(__file__).resolve().parents[1]

class Simulator:
    def __init__(self, executable):
        self.process = subprocess.Popen([str(Path(executable).resolve()), '--server', '--python', sys.executable, '--policy-service', str(ROOT / 'python/service.py')], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
        self.serial = 0

    def evaluate(self, weights, seed, scenario='peaceful', controller='neural', log_dir=''):
        self.serial += 1
        q = dict(v=1, type='evaluate_genome', request_id=self.serial, episode=self.serial, seed=seed, scenario=scenario, controller=controller, log_dir=str(log_dir))
        if weights is not None:
            q['weights'] = weights
        self.process.stdin.write(json.dumps(q, allow_nan=False) + '\n')
        self.process.stdin.flush()
        line = self.process.stdout.readline()
        if not line:
            raise RuntimeError('simulator exited unexpectedly')
        reply = json.loads(line)
        if reply.get('type') != 'episode_result' or reply.get('request_id') != self.serial:
            raise RuntimeError(str(reply))
        result = reply['result']
        if result['policy_failures']:
            raise RuntimeError('training rejected an episode using fallback')
        return result

    def close(self):
        self.process.stdin.close()
        try:
            self.process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait()
        self.process.stdout.close()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()

def partition(name):
    return json.loads((ROOT / 'scenarios' / (name + '.json')).read_text())

def tuples(x):
    return tuple(tuples(y) for y in x) if isinstance(x, list) else x

def atomic_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix('.tmp')
    temporary.write_text(json.dumps(value, allow_nan=False, indent=2))
    temporary.replace(path)

def breed(population, scores, rng):
    ranked = sorted(range(len(population)), key=lambda i: scores[i], reverse=True)
    children = [population[i][:] for i in ranked[:2]]
    def tournament():
        return population[max(rng.sample(range(len(population)), min(3, len(population))), key=lambda i: scores[i])]
    while len(children) < len(population):
        a, b = tournament(), tournament()
        child = [a[i] if rng.random() < .5 else b[i] for i in range(GENES)]
        children.append([max(-5, min(5, x + (rng.gauss(0, .2) if rng.random() < .08 else 0))) for x in child])
    return children

def train(args):
    if args.population < 2 or args.generations < 1:
        raise ValueError('population >= 2 and generations >= 1 required')
    output = Path(args.output)
    rng = random.Random(args.seed)
    champion, best, start, history = None, -float('inf'), 0, []
    if args.resume:
        checkpoint = json.loads(Path(args.resume).read_text())
        if checkpoint.get('schema_version') != 1 or checkpoint['training_scenarios'] != partition('train'):
            raise ValueError('incompatible checkpoint')
        population = [Policy(w).weights for w in checkpoint['population']]
        rng.setstate(tuples(checkpoint['rng_state']))
        champion, best, start, history = checkpoint['champion'], checkpoint['best_fitness'], checkpoint['generation'], checkpoint['history']
    else:
        population = [[rng.uniform(-1, 1) for _ in range(GENES)] for _ in range(args.population)]
    output.mkdir(parents=True, exist_ok=True)
    with Simulator(args.runtime) as sim:
        for generation in range(start, start + args.generations):
            scores = []
            for genome, weights in enumerate(population):
                episodes = [sim.evaluate(weights, case['seed'], case['scenario']) for case in partition('train')]
                scores.append(statistics.mean(e['fitness'] for e in episodes))
                with (output / 'episodes.csv').open('a', newline='') as f:
                    writer = csv.writer(f)
                    if f.tell() == 0:
                        writer.writerow(['generation', 'genome', 'seed', 'fitness', 'arrivals'])
                    for e in episodes:
                        writer.writerow([generation, genome, e['seed'], e['fitness'], sum(a['arrived'] for a in e['agents'])])
            winner = max(range(len(population)), key=scores.__getitem__)
            if scores[winner] > best:
                best, champion = scores[winner], population[winner][:]
            row = dict(generation=generation, best=max(scores), mean=statistics.mean(scores), stdev=statistics.pstdev(scores))
            history.append(row)
            print(json.dumps(row), flush=True)
            Policy(champion).save(output / 'champion.json')
            population = breed(population, scores, rng)
            atomic_json(output / 'latest.json', dict(schema_version=1, generation=generation + 1, population=population, rng_state=rng.getstate(), champion=champion, best_fitness=best, history=history, training_scenarios=partition('train')))
            with (output / 'generations.csv').open('w', newline='') as f:
                writer = csv.DictWriter(f, fieldnames=['generation', 'best', 'mean', 'stdev'])
                writer.writeheader()
                writer.writerows(history)
        validation = [sim.evaluate(champion, case['seed'], case['scenario']) for case in partition('validation')]
        atomic_json(output / 'validation.json', validation)
    plot(history, output / 'fitness_curve.png')

def plot(history, path):
    os.environ.setdefault("MPLCONFIGDIR", str(path.parent / ".matplotlib"))
    try:
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
    except ImportError:
        print('Install optional matplotlib for PNG charts; CSV metrics are saved.', file=sys.stderr)
        return
    xs = [r['generation'] for r in history]
    means = [r['mean'] for r in history]
    deviations = [r['stdev'] for r in history]
    plt.plot(xs, [r['best'] for r in history], label='Generation best')
    plt.plot(xs, means, label='Population mean')
    plt.fill_between(xs, [a-b for a,b in zip(means, deviations)], [a+b for a,b in zip(means, deviations)], alpha=.2)
    plt.xlabel('Generation'); plt.ylabel('Fitness'); plt.legend(); plt.tight_layout(); plt.savefig(path); plt.close()

def evaluate(args):
    output = Path(args.output)
    output.mkdir(parents=True, exist_ok=True)
    weights = Policy.load(args.model).weights
    rows, episodes = [], []
    with Simulator(args.runtime) as sim:
        for controller in ('neural', 'rule', 'random', 'neural-no-social'):
            samples = []
            durations = []
            for case in partition('test'):
                begin = time.perf_counter()
                e = sim.evaluate(weights if controller.startswith('neural') else None, case['seed'], case['scenario'], controller, output / 'traces' / f"{controller}-{case['seed']}")
                durations.append(time.perf_counter()-begin)
                samples.append(e)
                episodes.append(dict(controller=controller, scenario=case['scenario'], **e))
            agents = [a for e in samples for a in e['agents']]
            arrived = [a for a in agents if a['arrived']]
            fitnesses = [e['fitness'] for e in samples]
            rows.append(dict(controller=controller, arrival_alive_rate=sum(a['arrived'] and a['alive'] for a in agents)/len(agents), median_completion_ticks=statistics.median(a['ticks'] for a in arrived) if arrived else '', health_at_arrival=statistics.mean(a['health'] for a in arrived) if arrived else '', harmful_first_attacks=sum(a['first_attacks'] for a in agents), damage_received=sum(a['damage'] for a in agents), cooperative_shortcuts=sum(e['gate_open'] for e in samples), mean_path_cost=statistics.mean(a['path_cost'] for a in agents), arrivals_per_pickup=len(arrived)/max(1,sum(a['pickups'] for a in agents)), fitness_mean=statistics.mean(fitnesses), fitness_stdev=statistics.pstdev(fitnesses), mean_episode_seconds=statistics.mean(durations)))
    with (output / 'comparison.csv').open('w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0])); writer.writeheader(); writer.writerows(rows)
    atomic_json(output / 'episodes.json', episodes)
    print(json.dumps(rows, indent=2))

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=['train', 'evaluate'])
    parser.add_argument('--runtime', default=str(ROOT / 'build/GameRuntime'))
    parser.add_argument('--output', default=str(ROOT / 'data/run'))
    parser.add_argument('--seed', type=int, default=42)
    parser.add_argument('--population', type=int, default=30)
    parser.add_argument('--generations', type=int, default=10)
    parser.add_argument('--resume')
    parser.add_argument('--model')
    args = parser.parse_args()
    if args.command == 'evaluate' and not args.model:
        parser.error('--model is required for evaluation')
    train(args) if args.command == 'train' else evaluate(args)

if __name__ == '__main__':
    main()
