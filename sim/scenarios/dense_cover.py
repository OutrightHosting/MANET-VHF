"""
Dense cover spends the hop budget, and the whole test suite was blind to it.

Every scenario in this project is a variation on the repeater triangle -- groups spread
over kilometres, terrain doing the blocking, hops long and few. That is the case the
product was conceived for and it works.

It is not the only case. In thick woodland the links shorten, so the same group needs MORE
hops to span the SAME ground, and MANET_VOICE_TTL runs out before the group does. The
radios are all still connected to each other; voice just cannot get from one end to the
other. Nothing in the gate would notice, because every scenario it runs has long hops.

Worst measured case: twelve leaders strung along 2 km of very dense cover -- an entirely
ordinary way for a group to walk a forest trail -- with 12/12 radios reachable through the
mesh and the last FOUR of them receiving nothing at all.

  python3 -m sim.scenarios.dense_cover
"""
from ..manet.core import CONFIG
from ..manet.mobility import Static
from ..manet.radio import Environment, LinkBudget, usable_range_m
from ..manet.world import Simulation

BUD = LinkBudget()

# Cover densities, named by the single-hop range they produce rather than by an exponent,
# because the range is the thing anyone can picture.
# Each figure is a target HORIZON, the sweep's independent variable, not a distance pinned
# against one — env_for_horizon() derives an environment to match.
COVERS = (("light — our default", 4416.0),   # geometry-exempt: target horizon
          ("moderate", 1500.0),              # geometry-exempt: target horizon
          ("dense", 700.0),                  # geometry-exempt: target horizon
          ("very dense", 350.0))             # geometry-exempt: target horizon

SPREADS_M = (500.0, 1000.0, 2000.0, 3000.0, 5000.0)   # geometry-exempt: the sweep axis


def env_for_horizon(target_m):
    """The environment whose single-hop range is `target_m`. Bisection on the exponent."""
    lo, hi = 2.0, 9.0
    for _ in range(60):
        mid = (lo + hi) / 2.0
        if usable_range_m(Environment("x", exponent=mid, foliage=True), BUD) > target_m:
            lo = mid
        else:
            hi = mid
    return Environment(f"horizon {target_m:.0f} m", exponent=lo, foliage=True)


def line_of_leaders(n, spread_m, env, slots=3000):
    pos = [(i * spread_m / (n - 1), 0.0) for i in range(n)]
    sim = Simulation(Static(pos), env, BUD, talker=0)
    settle = CONFIG.beacon_interval_slots * 4
    sim.run(settle)
    sim.run(slots - settle, voice_from=settle)
    d = sim.delivery(sim.nodes[0].addr)
    return sim, d


if __name__ == "__main__":
    n = 12
    print(f"{n} leaders in a line. Worst-case delivery. TTL = {CONFIG.voice_ttl} hops.\n")
    head = " ".join(f"{s/1000:>6.1f}km" for s in SPREADS_M)
    print(f"{'cover':>20} {'1 hop':>8} | {head}")
    for label, target in COVERS:
        env = env_for_horizon(target)
        cells = []
        for spread in SPREADS_M:
            _, d = line_of_leaders(n, spread, env)
            w = min(d.values())
            cells.append(f"{w*100:>6.0f}%" + ("!" if w < 0.5 else " "))
        print(f"{label:>20} {target:>6.0f} m | " + " ".join(cells))
    print("\n  ! = part of the group is cut off from the rest\n")

    env = env_for_horizon(350.0)  # geometry-exempt: a target horizon, not a spacing
    sim, d = line_of_leaders(n, 2000.0, env, slots=4000)
    print("The corner that matters — 12 leaders over 2 km of very dense cover:")
    print(f"  radios reachable through the mesh : {sim.reachable_from(0)}/{n}")
    print(f"  radios actually receiving voice   : {sum(1 for v in d.values() if v > 0.5)}/{n}")
    print("\n  The radio path is intact end to end. Voice stops anyway, because the hop")
    print("  budget runs out before the group does — and the hop budget cannot simply be")
    print(f"  raised, since {CONFIG.voice_ttl} hops already costs 460 ms of a 500 ms")
    print("  mouth-to-ear allowance. Dense cover turns a LATENCY limit into a COVERAGE limit.")
