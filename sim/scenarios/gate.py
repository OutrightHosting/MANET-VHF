"""
The Phase 0 gate.

The brief sets five questions and three success criteria. This runs them and reports
pass or fail against each, so the gate is a command rather than a judgement call.

  Questions                                        Criteria
  1. Does MPR converge with 12 nodes moving?       converges with 12 mobile nodes
  2. Does the cluster case produce zero relaying?  cluster case relays nothing
  3. Does pipelining resolve across 3-5 hops?      5-hop chain within 300 ms
  4. Beacon overhead as % of channel capacity?
  5. What happens on partition and rejoin?
"""

from ..manet.core import CONFIG
from ..manet.mobility import DispersingGroup, SplinterRejoin
from ..manet.radio import ENVIRONMENTS, LinkBudget, usable_range_m
from ..manet.world import Simulation

WOOD = ENVIRONMENTS["woodland"]
BUDGET = LinkBudget()


def _secs(slots):
    return slots * CONFIG.slot_us / 1e6


def cluster(n=12, seconds=90):
    """Twelve leaders standing together. Nothing should relay."""
    from ..manet.mobility import DispersingGroup as DG
    mob = DG(n, spread_min=120.0, spread_max=120.0, period_s=1e9)
    sim = Simulation(mob, WOOD, BUDGET, talker=0)
    settle = CONFIG.beacon_interval_slots * 4
    sim.run(settle)
    total = int(seconds / (CONFIG.slot_us / 1e6))
    sim.run(total, voice_from=settle)

    d = sim.delivery(sim.nodes[0].addr)
    return {
        "relays_selected": sim.relaying_now(),
        "relay_transmissions": sim.relay_total(),
        "min_delivery": min(d.values()) if d else 0.0,
        "reachable": sim.reachable_from(0),
        "nodes": n,
    }


def dispersal(n=12, cycles=2, samples=60):
    """The group stretches out and gathers up again while someone is talking."""
    mob = DispersingGroup(n, spread_min=120.0, spread_max=3000.0, period_s=240.0)
    sim = Simulation(mob, WOOD, BUDGET, talker=0)
    settle = CONFIG.beacon_interval_slots * 4
    sim.run(settle)

    total = int(cycles * 240.0 / (CONFIG.slot_us / 1e6))
    step = max(total // samples, 1)
    trace, converged_samples = [], 0
    done = 0
    while done < total:
        chunk = min(step, total - done)
        sim.run(chunk, voice_from=settle, voice_to=settle + total)
        done += chunk
        t = _secs(sim.slot) - _secs(settle)
        spread = mob.spread_at(t)
        ok = sim.converged()
        converged_samples += 1 if ok else 0
        trace.append((t, spread, sim.relaying_now(), sim.reachable_from(0), ok))

    d = sim.delivery(sim.nodes[0].addr)
    return {
        "samples": len(trace),
        "converged_fraction": converged_samples / max(len(trace), 1),
        "trace": trace,
        "min_delivery": min(d.values()) if d else 0.0,
        "mean_delivery": sum(d.values()) / len(d) if d else 0.0,
        "relay_transmissions": sim.relay_total(),
    }


def partition(n=12, samples=60):
    """The group splits, goes out of contact, and comes back."""
    mob = SplinterRejoin(n, separation_m=4000.0, out_s=90.0, away_s=120.0, back_s=90.0)
    sim = Simulation(mob, WOOD, BUDGET, talker=0)
    settle = CONFIG.beacon_interval_slots * 4
    sim.run(settle)

    total = int(400.0 / (CONFIG.slot_us / 1e6))
    step = max(total // samples, 1)
    trace = []
    done = 0
    while done < total:
        chunk = min(step, total - done)
        sim.run(chunk, voice_from=settle, voice_to=settle + total)
        done += chunk
        t = _secs(sim.slot) - _secs(settle)
        trace.append((t, mob.phase_at(t), sim.reachable_from(0),
                      sim.reachable_from(n - 1), sim.converged()))

    # how long after the halves are back in contact before everything reconverges
    rejoin_t = 90.0 + 120.0 + 90.0
    heal = None
    for t, phase, reach_a, _reach_b, ok in trace:
        if t >= rejoin_t and reach_a == n and ok:
            heal = t - rejoin_t
            break
    return {"trace": trace, "heal_s": heal, "rejoin_at_s": rejoin_t, "nodes": n}


def latency(hops=6):
    """Slots, and milliseconds, from the talker's mouth to each relay down the chain."""
    reach = usable_range_m(WOOD, BUDGET)
    pos = [(i * 0.9 * reach, 0.0) for i in range(hops + 1)]
    sim = Simulation(pos, WOOD, BUDGET, talker=0)
    settle = CONFIG.beacon_interval_slots * 4
    sim.run(settle)
    sim.run(1200, voice_from=settle)
    lat = sim.latency_slots(sim.nodes[0].addr)
    return {i: (v, v * CONFIG.slot_us / 1000.0) for i, v in sorted(lat.items())}
