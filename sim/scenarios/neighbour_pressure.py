"""
Neighbour-table pressure: dense clusters strung out over several hops.

The flat-cluster case does NOT show this. Everyone hears the talker directly, so 30,000
refused table entries across 48 radios cost nothing measurable. The table only matters
where relay decisions depend on knowing who you uniquely reach, which needs density AND
multiple hops at once.

  python3 -m sim.scenarios.neighbour_pressure
"""
import sys, collections; sys.path.insert(0,'.')
from sim.manet.core import CONFIG
from sim.manet.geometry import hop_span_m
from sim.manet.mobility import Static
from sim.manet.radio import ENVIRONMENTS, LinkBudget
from sim.manet.terrain import Ridge
from sim.manet.world import Simulation
WOOD, BUD = ENVIRONMENTS["woodland"], LinkBudget()
ERR_BUFFER = 2

class Counting(Simulation):
    def _receive(self, rx, pdu, entries, dbm, slot):
        if rx.nb.heard(pdu.prev, self.channel.quality(dbm), slot) == ERR_BUFFER:
            self.refused[rx.index] += 1
        return super()._receive(rx, pdu, entries, dbm, slot)

def groups_scenario(groups, per_group=4, slots=4000):
    span = hop_span_m(WOOD, BUD, 3)
    step = span / max(groups - 1, 1)
    ridge = Ridge(crest_x=(groups // 2) * step, height_m=80.0, width_m=400.0)
    pos = []
    for g in range(groups):
        for i in range(per_group):
            pos.append((g*step + (i-1)*60.0, (i % 3)*50.0))
    sim = Counting(Static(pos), WOOD, BUD, talker=0, terrain=ridge)
    sim.refused = collections.Counter()
    s = CONFIG.beacon_interval_slots*4
    sim.run(s); sim.run(slots-s, voice_from=s)
    d = sim.delivery(sim.nodes[0].addr)
    return sim, d, len(pos)

print(f"table = {CONFIG.max_neighbours} entries, evict-worst (B-05)\n")
print(f"{'groups':>7} {'radios':>7} {'refused':>9} {'nodes hit':>10} {'reachable':>11} "
      f"{'worst':>8} {'mean':>8}")
for g in (6, 8, 10, 12):
    sim, d, n = groups_scenario(g)
    hit = sum(1 for v in sim.refused.values() if v > 0)
    print(f"{g:>7} {n:>7} {sum(sim.refused.values()):>9} {hit:>7}/{n:<3} "
          f"{sim.reachable_from(0):>7}/{n:<3} {min(d.values())*100:>7.1f}% "
          f"{sum(d.values())/len(d)*100:>7.1f}%")
