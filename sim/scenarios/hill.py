"""
The repeater triangle, formed by whoever happens to be standing on the hill.

Two groups at the bottom of a hill cannot hear each other — a ridge between them costs
more than the link budget has. Anyone on top hears both. So the moment one leader walks
over the crest, the two valleys are connected, and when they walk down the other side
someone else takes over. It is the geometry of a repeater on a hilltop, without the site,
the licence, or the forty-two working day lead time.

This is the mechanism the brief describes and the one the propagation model could not
express until terrain was added. Vegetation cannot do this: it saturates at about 11 dB.
A ridge does it easily.
"""

from ..manet.core import CONFIG
from ..manet.mobility import Static
from ..manet.geometry import hop_span_m, horizon_m, within_one_hop_m
from ..manet.radio import ENVIRONMENTS, LinkBudget
from ..manet.terrain import Ridge
from ..manet.world import Simulation

WOOD = ENVIRONMENTS["woodland"]
BUDGET = LinkBudget()

# Same defect the Phase 0 gate had until B-01/B-02: geometry hardcoded in metres against a
# radio horizon that moves. The groups were 1200 m apart and the crest at 1500 m, chosen when
# the horizon was 4416 m. At a shorter horizon -- and M-01 shows the plausible range spans
# 1.9 to 4.4 km -- the valleys cannot reach the hilltop and the scenario silently stops
# testing relaying at all. Derived from measured range instead, so it moves with the model.
RANGE_M = horizon_m(WOOD, BUDGET)
GROUP_SPACING = within_one_hop_m(WOOD, BUDGET)   # valley to hilltop, dependably
CREST_X = GROUP_SPACING * 1.0      # the ridge sits on the middle group


def three_groups(hilltop=True, per_group=4, spread=250.0, slots=1500):
    """
    Valley — hilltop — valley. With `hilltop=False` the middle group is removed, which
    should sever the network completely.
    """
    ridge = Ridge(crest_x=CREST_X, height_m=80.0, width_m=400.0)
    pos = []
    centres = ([0.0, GROUP_SPACING, 2 * GROUP_SPACING] if hilltop
               else [0.0, 2 * GROUP_SPACING])
    for c in centres:
        for i in range(per_group):
            pos.append((c + (i - per_group / 2) * spread / per_group, (i % 2) * 40.0))

    sim = Simulation(Static(pos), WOOD, BUDGET, talker=0, terrain=ridge)
    settle = CONFIG.beacon_interval_slots * 4
    sim.run(settle)
    sim.run(slots - settle, voice_from=settle)

    d = sim.delivery(sim.nodes[0].addr)
    far = list(range(len(pos)))[-per_group:]
    return {
        "nodes": len(pos),
        "reachable": sim.reachable_from(0),
        "relays": sim.relay_total(),
        "far_group": sum(d.get(i, 0.0) for i in far) / len(far),
        "delivery": d,
    }


def many_groups(groups=8, per_group=4, slots=6000):
    # 1500 slots does NOT reach steady state under barrage relaying: the worst node reads
    # 4.5% at 1500, 60.7% at 3000 and 80.5% at 6000, because the relay set depends on
    # neighbour tables that need several beacon intervals to fill. The election-based
    # version plateaued by 1500, which is why this default was never questioned.
    """
    The generalisation: scatter groups across a valley system and let whoever is high up
    do the relaying. Nobody is configured as a relay and nobody needs to be.
    """
    # Strung out over a span the topology can actually cross, derived rather than pinned.
    #
    # The crest sits ON a group, not between two. The whole scenario is "whoever is high up
    # relays", so if the ridge falls in a gap then nobody is standing on it and the network
    # simply severs -- 16/32 reachable, which is what the first version of this conversion
    # produced. Note the previous hardcoded version had the opposite failure: 371 m spacing
    # inside a 4416 m horizon meant every group heard every other directly, so it was a
    # cluster wearing a chain costume and the ridge was decorative.
    span = hop_span_m(WOOD, BUDGET, 3)
    step = span / max(groups - 1, 1)
    ridge = Ridge(crest_x=(groups // 2) * step, height_m=80.0, width_m=400.0)
    pos = []
    for g in range(groups):
        cx = g * step
        for i in range(per_group):
            pos.append((cx + (i - per_group / 2) * 60.0, (i % 3) * 50.0))

    sim = Simulation(Static(pos), WOOD, BUDGET, talker=0, terrain=ridge)
    settle = CONFIG.beacon_interval_slots * 4
    sim.run(settle)
    sim.run(slots - settle, voice_from=settle)
    d = sim.delivery(sim.nodes[0].addr)
    return {
        "nodes": len(pos),
        "reachable": sim.reachable_from(0),
        "relays": sim.relay_total(),
        "worst": min(d.values()) if d else 0.0,
        "mean": sum(d.values()) / len(d) if d else 0.0,
    }
