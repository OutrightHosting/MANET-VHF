"""
Where the leaders are, over time.

Everything simulated so far has been a static topology, which is the easy case. A mesh
either proves itself or falls apart when links break and reform underneath it, and that
is what these models are for.

Movement is deterministic. Lateral wander comes from a seeded PRNG so a failure
reproduces exactly — ADR-0006 applies to the harness as well as the core, because a
convergence bug that appears one run in twenty is worthless if it cannot be replayed.

Scales are taken from the operational picture in the brief: a dozen leaders on foot,
walking pace over rough ground, a group that stretches out along a path and gathers up
again, and splinter groups that go out of contact and come back.
"""

import math
import random

WALKING_MPS = 1.2   # rough ground, carrying kit — slower than road pace


class Mobility:
    def positions_at(self, t_us):
        raise NotImplementedError

    @property
    def count(self):
        raise NotImplementedError


class Static(Mobility):
    def __init__(self, positions):
        self._pos = list(positions)

    def positions_at(self, t_us):
        return self._pos

    @property
    def count(self):
        return len(self._pos)


class DispersingGroup(Mobility):
    """
    A group walking a path while stretching out and gathering up again.

    This is the normal case the product exists for: the front of the group pulls away,
    the back falls behind, and the network has to grow relays as it happens and shed them
    again when everyone closes up. Over one cycle it passes through the clustered case
    (nothing should relay) and the strung-out case (relaying throughout) without anyone
    touching a radio.
    """

    def __init__(self, n, spread_min=120.0, spread_max=3000.0, period_s=240.0,
                 speed_mps=WALKING_MPS, wander_m=40.0, seed=1):
        self.n = n
        self.spread_min = spread_min
        self.spread_max = spread_max
        self.period_s = period_s
        self.speed = speed_mps
        rng = random.Random(seed)
        # Each leader keeps a personal offset from the group's centre line, and a
        # personal rate of wandering about it. Nobody walks a perfect line.
        self._phase = [rng.uniform(0.0, 2.0 * math.pi) for _ in range(n)]
        self._rate = [rng.uniform(0.02, 0.06) for _ in range(n)]
        self._amp = [rng.uniform(0.3, 1.0) * wander_m for _ in range(n)]

    def spread_at(self, t_s):
        half = 0.5 * (1.0 - math.cos(2.0 * math.pi * t_s / self.period_s))
        return self.spread_min + (self.spread_max - self.spread_min) * half

    def positions_at(self, t_us):
        t = t_us / 1e6
        head = self.speed * t
        spread = self.spread_at(t)
        out = []
        for i in range(self.n):
            frac = i / max(self.n - 1, 1)
            x = head - spread * frac
            y = self._amp[i] * math.sin(self._phase[i] + self._rate[i] * t)
            out.append((x, y))
        return out

    @property
    def count(self):
        return self.n


class SplinterRejoin(Mobility):
    """
    The group divides, one half goes out of contact, and later comes back.

    Tests the partition and rejoin question directly. While separated, each half should
    keep working on its own; on return, the two halves must find each other and
    reconverge with nobody pressing anything.
    """

    def __init__(self, n, separation_m=4000.0, out_s=120.0, away_s=120.0,
                 back_s=120.0, cluster_m=150.0, speed_mps=WALKING_MPS, seed=2):
        self.n = n
        self.split = n // 2
        self.separation = separation_m
        self.out_s, self.away_s, self.back_s = out_s, away_s, back_s
        rng = random.Random(seed)
        self._jit = [(rng.uniform(-cluster_m, cluster_m),
                      rng.uniform(-cluster_m, cluster_m)) for _ in range(n)]
        self.speed = speed_mps

    def phase_at(self, t_s):
        if t_s < self.out_s:
            return "separating"
        if t_s < self.out_s + self.away_s:
            return "apart"
        if t_s < self.out_s + self.away_s + self.back_s:
            return "returning"
        return "rejoined"

    def _offset(self, t_s):
        if t_s < self.out_s:
            return self.separation * (t_s / self.out_s)
        if t_s < self.out_s + self.away_s:
            return self.separation
        if t_s < self.out_s + self.away_s + self.back_s:
            back = (t_s - self.out_s - self.away_s) / self.back_s
            return self.separation * (1.0 - back)
        return 0.0

    def positions_at(self, t_us):
        t = t_us / 1e6
        off = self._offset(t)
        out = []
        for i in range(self.n):
            jx, jy = self._jit[i]
            x = jx + (off if i >= self.split else 0.0)
            out.append((x, jy))
        return out

    @property
    def count(self):
        return self.n
