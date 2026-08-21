"""
Terrain, and the diffraction loss it causes.

The thing the propagation model has been missing, and the reason every reach figure so far
has been suspect. Attenuation through vegetation is gentle and saturates at about 11 dB
(ITU-R P.833-10). Terrain is not gentle: a ridge between two radios costs tens of dB and is
the mechanism the brief actually describes — *"a ridge or 200 m of woodland kills a link
that would work over open ground."*

It is also the mechanism that makes the product work. Two groups at the bottom of adjacent
valleys cannot hear each other at all. Both can hear anyone standing on the ridge between
them. That is a repeater on a hill, except nobody sited it, nobody licensed it, and it is
whoever happens to be up there at the time.

Diffraction uses the ITU-R P.526 single knife-edge approximation, which is the standard
first-order treatment for an obstructed terrestrial path.
"""

import math

C_LIGHT = 299792458.0
ANTENNA_HEIGHT_M = 1.5   # a handheld held at chest height


class Terrain:
    """A height field. Flat by default; subclasses give it shape."""

    def height(self, x, y):
        return 0.0

    def profile(self, a, b, samples=64):
        """Ground heights sampled along the straight line from a to b."""
        return [self.height(a[0] + (b[0] - a[0]) * i / samples,
                            a[1] + (b[1] - a[1]) * i / samples)
                for i in range(samples + 1)]


class Flat(Terrain):
    pass


class Ridge(Terrain):
    """
    A single ridge running across the area — the hill between two valleys.

    Gaussian in cross-section, which is smooth enough for knife-edge diffraction to be a
    reasonable first approximation and shaped enough to actually block a path.
    """

    def __init__(self, crest_x, height_m, width_m):
        self.crest_x = crest_x
        self.height_m = height_m
        self.width_m = width_m

    def height(self, x, y):
        d = (x - self.crest_x) / self.width_m
        return self.height_m * math.exp(-d * d)


class Ridges(Terrain):
    """Several ridges — a valley system rather than a single hill."""

    def __init__(self, ridges):
        self.ridges = list(ridges)

    def height(self, x, y):
        return max((r.height(x, y) for r in self.ridges), default=0.0)


def knife_edge_db(nu):
    """
    ITU-R P.526 single knife-edge diffraction loss for Fresnel parameter nu.

    Below -0.78 the path is effectively clear and the loss is taken as zero.
    """
    if nu < -0.78:
        return 0.0
    return 6.9 + 20.0 * math.log10(math.sqrt((nu - 0.1) ** 2 + 1.0) + nu - 0.1)


def diffraction_db(a, b, terrain, freq_hz, samples=64):
    """
    Excess loss from terrain obstructing the path between two radios.

    Walks the ground profile, finds the point that intrudes furthest into the direct ray,
    and applies knife-edge diffraction there. Returns 0 for a clear path.
    """
    lam = C_LIGHT / freq_hz
    total = math.hypot(b[0] - a[0], b[1] - a[1])
    if total < 1.0:
        return 0.0

    h_a = terrain.height(a[0], a[1]) + ANTENNA_HEIGHT_M
    h_b = terrain.height(b[0], b[1]) + ANTENNA_HEIGHT_M
    ground = terrain.profile(a, b, samples)

    worst = None
    for i, g in enumerate(ground):
        if i == 0 or i == samples:
            continue
        d1 = total * i / samples
        d2 = total - d1
        # height of the obstacle above the straight line between the two antennas
        ray = h_a + (h_b - h_a) * (d1 / total)
        h = g - ray
        nu = h * math.sqrt(2.0 / lam * (1.0 / d1 + 1.0 / d2))
        if worst is None or nu > worst:
            worst = nu

    return knife_edge_db(worst) if worst is not None else 0.0
