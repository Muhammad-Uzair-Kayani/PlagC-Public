"""Dependency-free 16-16-6 neural policy. Feature order is docs/contract.md."""
import json
import math
import random
from pathlib import Path

INTENTS = ('Explore', 'Acquire', 'Cooperate', 'Avoid', 'Defend', 'Progress')
GENES = 374

def validate_weights(weights):
    if not isinstance(weights, list) or len(weights) != GENES:
        raise ValueError('expected 374 weights')
    if any(isinstance(x, bool) or not isinstance(x, (int, float)) or not math.isfinite(x) or abs(x) > 100 for x in weights):
        raise ValueError('weights must be finite numbers within [-100,100]')
    return list(weights)

class Policy:
    def __init__(self, weights=None, seed=1):
        self.weights = validate_weights(weights if weights is not None else [random.Random(seed + i).uniform(-1, 1) for i in range(GENES)])

    def choose(self, features, mask):
        if not isinstance(features, list) or len(features) != 16 or any(isinstance(x, bool) or not isinstance(x, (int, float)) or not math.isfinite(x) or not 0 <= x <= 1 for x in features):
            raise ValueError('expected 16 finite normalized features')
        if not isinstance(mask, list) or len(mask) != 6 or any(type(x) is not int or x not in (0, 1) for x in mask) or not any(mask):
            raise ValueError('invalid action mask')
        w = self.weights
        hidden = [math.tanh(sum(features[k] * w[j * 16 + k] for k in range(16)) + w[256 + j]) for j in range(16)]
        scores = [sum(hidden[k] * w[272 + j * 16 + k] for k in range(16)) + w[368 + j] for j in range(6)]
        return INTENTS[max((i for i in range(6) if mask[i]), key=lambda i: scores[i])]

    def save(self, path):
        path = Path(path)
        path.parent.mkdir(parents=True, exist_ok=True)
        temp = path.with_suffix(path.suffix + '.tmp')
        temp.write_text(json.dumps({'schema_version': 1, 'architecture': [16, 16, 6], 'weights': self.weights}, allow_nan=False))
        temp.replace(path)

    @classmethod
    def load(cls, path):
        data = json.loads(Path(path).read_text())
        if data.get('schema_version') != 1 or data.get('architecture') != [16, 16, 6]:
            raise ValueError('unsupported model schema/architecture')
        return cls(data['weights'])
