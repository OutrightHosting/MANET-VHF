"""
How long the network takes to knit itself back together after a partition heals.

B-06. The Phase 0 gate measures this at twelve radios and reports ~5 s, which is the easy
case. An adversarial sweep found barrage far slower than the election at
32 radios -- 7.6x -- and holding only 6 of 16 cross-boundary links at +30 s where the
election held all 16.

Three minutes of a safety network not working is a product problem, not a metric.

  python3 -m sim.scenarios.reconvergence
"""
import sys

from ..manet.core import CONFIG
from ..manet.geometry import hop_span_m, severed_m
from ..manet.mobility import Mobility, WALKING_MPS
from ..manet.radio import ENVIRONMENTS, LinkBudget
from ..manet.world import Simulation

WOOD, BUD = ENVIRONMENTS["woodland"], LinkBudget()
CLUSTER_JITTER_M = 120.0   # geometry-exempt: scatter within a group, a physical fact


class SplitAndRestore(Mobility):
    """Two halves, apart for a while, then abruptly back in contact."""

    def __init__(self, n, split_at_s, restore_s, seed=5):
        import random
        self.n = n
        self.split = n // 2
        self.split_at_s = split_at_s    # together before this: the baseline is measured here
        self.restore_s = restore_s
        self.sep = severed_m(WOOD, BUD, jitter_m=CLUSTER_JITTER_M)
        self.spread = hop_span_m(WOOD, BUD, 2)
        rng = random.Random(seed)
        self._j = [(rng.uniform(-CLUSTER_JITTER_M, CLUSTER_JITTER_M),
                    rng.uniform(-CLUSTER_JITTER_M, CLUSTER_JITTER_M)) for _ in range(n)]
        self.speed = WALKING_MPS

    def positions_at(self, t_us):
        t = t_us / 1e6
        # together -> apart -> together. The baseline has to be measured while joined, or
        # "restored" is compared against a split network and everything looks instant.
        apart = self.split_at_s <= t < self.restore_s
        off = self.sep if apart else 0.0
        out = []
        for i in range(self.n):
            jx, jy = self._j[i]
            half = 0 if i < self.split else 1
            within = (i % self.split) * (self.spread / max(self.split - 1, 1))
            out.append((jx + within + (off if half else 0.0), jy))
        return out

    @property
    def count(self):
        return self.n


def cross_links(sim, split):
    """Symmetric links that span the boundary — the thing that has to come back."""
    by_addr = {n.addr: n.index for n in sim.nodes}
    live = 0
    for n in sim.nodes:
        for a in n.nb.symmetric():
            j = by_addr.get(a)
            if j is not None and (n.index < split) != (j < split):
                live += 1
    return live // 2


def run(n=32, split_at_s=60.0, restore_s=180.0, watch_s=240.0, barrage=True):
    from ..manet.radio import Channel
    Channel.CONCURRENT_IDENTICAL = barrage
    Simulation.BARRAGE_RELAY = barrage
    mob = SplitAndRestore(n, split_at_s=split_at_s, restore_s=restore_s)
    sim = Simulation(mob, WOOD, BUD, talker=0)
    settle = CONFIG.beacon_interval_slots * 4
    sim.run(settle)
    baseline = cross_links(sim, n // 2)

    total = int((restore_s + watch_s) / (CONFIG.slot_us / 1e6))
    step = max(int(2.0 / (CONFIG.slot_us / 1e6)), 1)
    trace, done = [], 0
    while done < total:
        chunk = min(step, total - done)
        sim.run(chunk, voice_from=settle)
        done += chunk
        t = (sim.slot - settle) * CONFIG.slot_us / 1e6
        trace.append((t, cross_links(sim, n // 2)))
    return baseline, trace


def restore_time(baseline, trace, restore_s, frac):
    want = baseline * frac
    for t, live in trace:
        if t >= restore_s and live >= want:
            return t - restore_s
    return None


if __name__ == "__main__":
    n = 32
    restore_s = 180.0  # geometry-exempt: seconds, not metres
    print(f"{n} radios: together, split at t=60s, restored at t={restore_s:.0f}s\n")
    print(f"{'':10} {'joined':>7} {'apart':>6} {'90% back':>10} {'100% back':>11} {'@ +30s':>9}")
    for label, barrage in (("election", False), ("barrage", True)):
        base, trace = run(n=n, restore_s=restore_s, barrage=barrage)
        apart = min(l for t, l in trace if 70.0 <= t < restore_s)
        t90 = restore_time(base, trace, restore_s, 0.9)
        t100 = restore_time(base, trace, restore_s, 1.0)
        at30 = next((l for t, l in trace if t >= restore_s + 30.0), 0)
        f = lambda v: f"{v:.0f} s" if v is not None else "never"
        print(f"{label:10} {base:>7} {apart:>6} {f(t90):>10} {f(t100):>11} {at30:>5}/{base:<3}")
