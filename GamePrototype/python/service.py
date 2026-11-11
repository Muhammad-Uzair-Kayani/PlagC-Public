"""Persistent NDJSON service. stdout contains protocol responses only."""
import json
import sys
from neural import Policy

class Service:
    def __init__(self):
        self.policy = Policy()
        self.revision = 0

    def handle(self, q):
        if not isinstance(q, dict) or type(q.get('v')) is not int or q.get('v') != 1:
            raise ValueError('unsupported schema')
        if type(q.get('request_id')) is not int or q['request_id'] < 0 or type(q.get('episode')) is not int:
            raise ValueError('invalid correlation IDs')
        reply = {'v': 1, 'request_id': q['request_id'], 'episode': q['episode']}
        command = q.get('type')
        if command == 'hello':
            reply.update(type='ready', features=16, intents=6, genes=374)
        elif command == 'load_policy':
            policy = Policy(q['weights']) if 'weights' in q else Policy.load(q['path'])
            self.policy = policy
            self.revision += 1
            reply.update(type='loaded', policy_revision=self.revision)
        elif command in ('infer', 'infer_batch', 'evaluate_genome'):
            agents = q.get('agents')
            if not isinstance(agents, list) or len(agents) > 256:
                raise ValueError('invalid agent batch')
            ids = set()
            actions = []
            for a in agents:
                if not isinstance(a, dict) or type(a.get('id')) is not int or a['id'] <= 0 or a['id'] in ids:
                    raise ValueError('invalid/duplicate agent ID')
                ids.add(a['id'])
                actions.append({'id': a['id'], 'intent': self.policy.choose(a['features'], a['valid_actions']), 'target_id': 0, 'policy_revision': self.revision})
            reply.update(type='intents', actions=actions)
        elif command == 'save_checkpoint':
            self.policy.save(q['path'])
            reply['type'] = 'saved'
        elif command in ('reset_episode', 'episode_result'):
            reply['type'] = 'ack'
        elif command == 'shutdown':
            reply['type'] = 'bye'
        else:
            raise ValueError('unknown command')
        return reply

def main():
    service = Service()
    for line in sys.stdin:
        q = {}
        try:
            if len(line) > 1024 * 1024:
                raise ValueError('message too large')
            q = json.loads(line)
            reply = service.handle(q)
        except (ValueError, KeyError, TypeError, OSError) as exc:
            reply = {'v': 1, 'type': 'error', 'request_id': q.get('request_id') if isinstance(q, dict) else None, 'episode': q.get('episode') if isinstance(q, dict) else None, 'error': str(exc)}
        print(json.dumps(reply, allow_nan=False), flush=True)
        if reply['type'] == 'bye':
            break

if __name__ == '__main__':
    main()
