"""Example policy loaded by the C++ client through embedded Python.

This is a deterministic shortest-path baseline, not a trained policy. Replace
choose_action with neural-policy inference while preserving this small contract.
"""

from collections import deque


def choose_action(observation: dict) -> str:
    """Return one of north/east/south/west/wait from a plain state dictionary."""
    start = tuple(observation["position"])
    goal = tuple(observation["goal"])
    rows = observation["map"]
    if start == goal:
        return "wait"

    # The order makes equal-cost paths reproducible.
    moves = (
        ("north", 0, -1),
        ("east", 1, 0),
        ("south", 0, 1),
        ("west", -1, 0),
    )
    queue = deque([start])
    first_action = {start: None}
    height = len(rows)
    width = len(rows[0]) if height else 0

    while queue:
        x, y = queue.popleft()
        for action, dx, dy in moves:
            next_position = (x + dx, y + dy)
            nx, ny = next_position
            if not (0 <= nx < width and 0 <= ny < height):
                continue
            if rows[ny][nx] == "#" or next_position in first_action:
                continue
            first_action[next_position] = action if (x, y) == start else first_action[(x, y)]
            if next_position == goal:
                return first_action[next_position]
            queue.append(next_position)

    return "wait"
