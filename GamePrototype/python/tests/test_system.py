import json
import math
import os
from pathlib import Path
import random
import subprocess
import sys
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'python'))
from neural import GENES, Policy
from service import Service
from train import breed, partition, Simulator

class PolicyTests(unittest.TestCase):
    def test_network_roundtrip_mask(self):
        p = Policy(seed=3)
        self.assertEqual(len(p.weights), GENES)
        self.assertEqual(p.choose([.5]*16, [0,0,0,0,0,1]), 'Progress')
        with tempfile.TemporaryDirectory() as d:
            p.save(Path(d)/'p.json')
            self.assertEqual(p.weights, Policy.load(Path(d)/'p.json').weights)
        with self.assertRaises(ValueError): Policy([0]*373)
        with self.assertRaises(ValueError): Policy([math.nan]*374)
        for features, mask in [([0]*15,[1]*6),([math.inf]*16,[1]*6),([0]*16,[0]*6),([0]*16,[1]*5)]:
            with self.assertRaises(ValueError): p.choose(features, mask)

    def test_protocol_validation(self):
        s = Service()
        q = dict(v=1,type='infer_batch',request_id=42,episode=7,agents=[dict(id=i,features=[.5]*16,valid_actions=[1]*6) for i in (2,3)])
        a=s.handle(q)
        self.assertEqual([x['id'] for x in a['actions']],[2,3])
        self.assertEqual(a,s.handle(q))
        for update in [dict(v=2),dict(request_id=-1),dict(agents=[q['agents'][0]]*2),dict(type='bad')]:
            with self.assertRaises(ValueError): s.handle(dict(q,**update))
        p = subprocess.run([sys.executable,str(ROOT/'python/service.py')],input='bad json\n'+json.dumps(dict(v=1,type='hello',request_id=2,episode=1))+'\n',text=True,capture_output=True,check=True)
        self.assertEqual([json.loads(l)['type'] for l in p.stdout.splitlines()],['error','ready'])

    def test_ga_and_partitions(self):
        pop=[Policy(seed=i).weights for i in range(6)]
        a=breed(pop,list(range(6)),random.Random(2))
        b=breed(pop,list(range(6)),random.Random(2))
        self.assertEqual(a,b)
        self.assertEqual(a[0],pop[5])
        self.assertEqual(a[1],pop[4])
        groups=[{x['seed'] for x in partition(p)} for p in ('train','validation','test')]
        self.assertFalse(groups[0]&groups[1] or groups[0]&groups[2] or groups[1]&groups[2])

RUNTIME=os.environ.get('SURVIVAL_RUNTIME',str(ROOT/'build/GameRuntime'))
@unittest.skipUnless(Path(RUNTIME).exists(),'build runtime first')
class IntegrationTests(unittest.TestCase):
    def test_cpp_neural_and_determinism(self):
        with Simulator(RUNTIME) as sim:
            weights=[0.0]*374;weights[373]=5
            a=sim.evaluate(weights,901)
            b=sim.evaluate(weights,901)
            self.assertEqual(a,b)
            self.assertTrue(all(x['arrived'] for x in a['agents']))
            self.assertEqual(a['policy_failures'],0)

    def test_transport_failure_fallback(self):
        # Constructor failure, malformed replies, late replies, unknown/missing/duplicate
        # actions and masked intents must never corrupt authoritative world state.
        bodies = [
            'print("garbage",flush=True)',
            'q["request_id"]+=1; print(json.dumps(q),flush=True)',
            'q["v"]=2; print(json.dumps(q),flush=True)',
            'print(json.dumps(dict(q,type="intents",actions=[])),flush=True)',
            'print(json.dumps(dict(q,type="intents",actions=[dict(id=1,intent="Invalid",target_id=0)]*5)),flush=True)',
            'print(json.dumps(dict(q,type="intents",actions=[dict(id=1,intent="Progress",target_id=0)]*5)),flush=True)',
            'time.sleep(3)',
        ]
        for body in bodies:
            with self.subTest(body=body),tempfile.TemporaryDirectory() as d:
                script=Path(d)/'bad.py'
                script.write_text('import sys,json,time\nfor line in sys.stdin:\n q=json.loads(line)\n if q["type"]=="hello": print(json.dumps(dict(q,type="ready")),flush=True); continue\n '+body+'\n')
                run=subprocess.run([RUNTIME,'--headless','--controller','neural','--python',sys.executable,'--policy-service',str(script)],text=True,capture_output=True,timeout=10,check=True)
                result=json.loads(run.stdout)
                self.assertGreater(result['policy_failures'],0)
                self.assertTrue(all(a['arrived'] for a in result['agents']))

    def test_checkpoint_resume(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d)
            full=root/'full'; split=root/'split'
            base=[sys.executable,str(ROOT/'python/train.py'),'train','--runtime',RUNTIME,'--population','4','--seed','5']
            subprocess.run(base+['--generations','2','--output',str(full)],check=True,capture_output=True)
            subprocess.run(base+['--generations','1','--output',str(split)],check=True,capture_output=True)
            subprocess.run(base+['--generations','1','--resume',str(split/'latest.json'),'--output',str(split)],check=True,capture_output=True)
            self.assertEqual(json.loads((full/'latest.json').read_text()),json.loads((split/'latest.json').read_text()))
            self.assertEqual((full/'champion.json').read_text(),(split/'champion.json').read_text())

    def test_unknown_cli(self):
        p=subprocess.run([RUNTIME,'--scenario','unknown','--headless'],capture_output=True)
        self.assertNotEqual(p.returncode,0)

if __name__=='__main__': unittest.main()
