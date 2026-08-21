"""
How far does this scale, and what stops it?

The product is specified for twelve leaders. This finds the ceilings anyway, because
knowing which of them is cheap to raise and which is fundamental is worth having before
anyone asks for a bigger group.
"""

from ..manet.core import CONFIG
from ..manet.mobility import DispersingGroup
from ..manet.radio import ENVIRONMENTS, LinkBudget, usable_range_m
from ..manet.world import Simulation

WOOD = ENVIRONMENTS["woodland"]
BUDGET = LinkBudget()


def run(n, spacing_frac=0.55, slots=1200):
    reach = usable_range_m(WOOD, BUDGET)
    spread = spacing_frac * reach * (n - 1)
    mob = DispersingGroup(n, spread_min=spread, spread_max=spread, period_s=1e9)
    sim = Simulation(mob, WOOD, BUDGET, talker=0)
    settle = CONFIG.beacon_interval_slots * 3
    sim.run(settle)
    sim.run(slots, voice_from=settle)
    d = sim.delivery(sim.nodes[0].addr)
    return {
        "nodes": n,
        "hops": round(spread / (0.9 * reach)),
        "beacon_airtime_pct": n / CONFIG.beacon_interval_slots * 100.0,
        "reachable": sim.reachable_from(0),
        "tables_full": sum(1 for x in sim.nodes if x.nb.count >= CONFIG.max_neighbours),
        "collisions": sim.collisions,
        "delivery": sum(d.values()) / len(d) if d else 0.0,
    }


def dense(n, spread_m=150.0, slots=1200):
    """Everyone in direct range of everyone — the case that saturates the tables."""
    mob = DispersingGroup(n, spread_min=spread_m, spread_max=spread_m, period_s=1e9)
    sim = Simulation(mob, WOOD, BUDGET, talker=0)
    settle = CONFIG.beacon_interval_slots * 3
    sim.run(settle)
    sim.run(slots, voice_from=settle)
    d = sim.delivery(sim.nodes[0].addr)
    return {
        "nodes": n,
        "reachable_via_routing": sim.reachable_from(0),
        "tables_full": sum(1 for x in sim.nodes if x.nb.count >= CONFIG.max_neighbours),
        "collisions": sim.collisions,
        "voice_delivery": sum(d.values()) / len(d) if d else 0.0,
    }
