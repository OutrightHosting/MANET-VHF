"""
TX duty cycle per node — the cost of every handset also being a relay.

A commercial handheld is specified to 5/5/90: 5% transmit, 5% receive, 90% standby. This
counts what the pivotal relay in a mesh actually does, which is rather more, and does so
while nobody is pressing its PTT. Feeds OQ-0026.

    python3 -m sim.scenarios.duty
"""

import collections, sys
from ..manet.core import CONFIG
from ..manet.mobility import Static
from ..manet.radio import ENVIRONMENTS, LinkBudget
from ..manet.geometry import hop_span_m, within_one_hop_m
from ..manet.terrain import Ridge
from ..manet.world import Simulation

WOOD, BUDGET = ENVIRONMENTS["woodland"], LinkBudget()
print(CONFIG)
burst_frac = CONFIG.burst_us / CONFIG.slot_us
PA_W = 5.0 / 0.45           # 5 W RF out of a ~45%-efficient VHF module

def build(pos, slots=1500):
    ridge = Ridge(crest_x=within_one_hop_m(WOOD, BUD), height_m=80.0, width_m=400.0)
    sim = Simulation(Static(pos), WOOD, BUDGET, talker=0, terrain=ridge)
    settle = CONFIG.beacon_interval_slots * 4
    sim.run(settle); sim.run(slots - settle, voice_from=settle)
    return sim

def three(per_group=4, spread=None):
    spread = spread if spread is not None else within_one_hop_m(WOOD, BUD) * 0.1
    pos = []
    G = within_one_hop_m(WOOD, BUD)
    for c in (0.0, G, 2 * G):
        for i in range(per_group):
            pos.append((c + (i - per_group / 2) * spread / per_group, (i % 2) * 40.0))
    return pos

def many(groups=8, per_group=4):
    pos = []
    for g in range(groups):
        cx = g * (hop_span_m(WOOD, BUD, 3) / max(groups - 1, 1))
        for i in range(per_group):
            pos.append((cx + (i - per_group / 2) * 60.0, (i % 3) * 50.0))
    return pos

for name, pos in (("3 groups over a hill (1 talker)", three()),
                  ("8 groups strung out  (1 talker)", many())):
    sim = build(pos); n = len(pos)
    total = max(s for s, _ in sim.tx_log) + 1
    per, kinds = collections.Counter(), collections.Counter()
    for slot, txs in sim.tx_log:
        for i, src, seq, typ in txs:
            per[i] += 1; kinds[typ] += 1
    duty = sorted((per[i] / total) * burst_frac for i in range(n))
    print(f"\n{name}: {n} nodes, {total} slots, {sum(per.values())} bursts {dict(kinds)}")
    print(f"  TX duty   median {duty[n//2]*100:5.1f}%   worst {duty[-1]*100:5.1f}%")
    print(f"  PA draw   worst node {duty[-1]*PA_W:5.2f} W average, {PA_W:.1f} W while keyed")
