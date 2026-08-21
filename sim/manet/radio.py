"""
Radio propagation and the shared channel.

This is the part of the harness that decides whether the answers mean anything. The
protocol logic is exact — it is the shipping C code — but everything it is judged
against comes from the model below.

Two honest caveats, stated here rather than buried:

  * The path-loss exponents are calibrated to give plausible handheld-to-handheld ranges
    at VHF (about 6 km over open ground, about 550 m through dense woodland), not
    derived from measurement. Phase 2 exists to replace them.
  * Terrain is modelled as an environment applied uniformly to every link, not as real
    topography. A ridge between two specific radios is not represented.

What the model does take seriously is the distinction that decides OQ-0013: a signal can
be far too weak to decode and still be strong enough to ruin someone else's reception.
Anything that collapses that into "in range / out of range" cannot answer the question.
"""

import math
from dataclasses import dataclass

C_LIGHT = 299792458.0


@dataclass(frozen=True)
class LinkBudget:
    """Everything between the transmitter's PA and the receiver's demodulator."""

    freq_hz: float = 155e6
    tx_dbm: float = 37.0        # 5 W, per the brief
    tx_gain_dbi: float = -4.0   # ~18 cm helical, not the 48 cm quarter wave
    rx_gain_dbi: float = -4.0
    body_loss_db: float = 4.0   # per end — belt-clipped against a torso
    sensitivity_dbm: float = -116.0
    capture_db: float = 10.0    # C/I a 4FSK demodulator needs to hold the wanted signal

    @property
    def eirp_dbm(self):
        return self.tx_dbm + self.tx_gain_dbi - self.body_loss_db

    @property
    def rx_offset_db(self):
        return self.rx_gain_dbi - self.body_loss_db

    @property
    def max_path_loss_db(self):
        return self.eirp_dbm + self.rx_offset_db - self.sensitivity_dbm


@dataclass(frozen=True)
class Environment:
    name: str
    exponent: float   # log-distance path loss exponent
    foliage: bool     # apply the ITU-R P.833 vegetation term over the whole path

    def path_loss_db(self, d_m, freq_hz):
        d = max(float(d_m), 1.0)
        lam = C_LIGHT / freq_hz
        loss = 20.0 * math.log10(4.0 * math.pi / lam) + 10.0 * self.exponent * math.log10(d)
        if self.foliage:
            # ITU-R P.833 in-leaf specific attenuation. Assumes the whole path is
            # vegetation, which is the point of the woodland case.
            f_mhz = freq_hz / 1e6
            loss += 0.2 * (f_mhz ** 0.3) * (d ** 0.6)
        return loss


# Calibrated so handheld-to-handheld range lands where field experience puts it.
OPEN = Environment("open moorland", exponent=3.2, foliage=False)
WOODLAND = Environment("dense woodland", exponent=3.0, foliage=True)
ENVIRONMENTS = {"open": OPEN, "woodland": WOODLAND}


def usable_range_m(env, budget, lo=1.0, hi=200000.0):
    """Distance at which the wanted signal just reaches the demodulator."""
    limit = budget.max_path_loss_db
    for _ in range(80):
        mid = (lo + hi) / 2.0
        if env.path_loss_db(mid, budget.freq_hz) < limit:
            lo = mid
        else:
            hi = mid
    return lo


def _dbm_to_mw(dbm):
    return 10.0 ** (dbm / 10.0)


class Channel:
    """
    The shared medium.

    One transmission per slot per radio; several radios may transmit in the same slot,
    which is the entire basis of spatial reuse. A receiver decodes the strongest signal
    if it clears the noise floor AND beats everything else by the capture margin.
    Otherwise it hears mush.
    """

    def __init__(self, env, budget=None):
        self.env = env
        self.budget = budget or LinkBudget()
        self._loss_cache = {}

    def rx_dbm(self, distance_m):
        key = round(distance_m, 1)
        loss = self._loss_cache.get(key)
        if loss is None:
            loss = self.env.path_loss_db(key, self.budget.freq_hz)
            self._loss_cache[key] = loss
        return self.budget.eirp_dbm + self.budget.rx_offset_db - loss

    def decode(self, rx_pos, transmissions, positions):
        """
        What this receiver gets out of one slot.

        transmissions: list of (tx_index, payload)
        Returns (tx_index, payload, rx_dbm) or None.
        """
        if not transmissions:
            return None

        powers = []
        for idx, payload in transmissions:
            tx = positions[idx]
            d = math.hypot(rx_pos[0] - tx[0], rx_pos[1] - tx[1])
            powers.append((self.rx_dbm(d), idx, payload))

        powers.sort(key=lambda t: -t[0])
        best_dbm, best_idx, best_payload = powers[0]

        if best_dbm < self.budget.sensitivity_dbm:
            return None  # nothing audible at all

        if len(powers) > 1:
            interference_mw = sum(_dbm_to_mw(p[0]) for p in powers[1:])
            if interference_mw > 0.0:
                interference_dbm = 10.0 * math.log10(interference_mw)
                if (best_dbm - interference_dbm) < self.budget.capture_db:
                    # Two transmissions of comparable strength. Neither survives — and
                    # this is precisely the failure OQ-0013 is asking about.
                    return None

        return (best_idx, best_payload, best_dbm)

    def quality(self, rx_dbm):
        """Map received power onto the 0..255 link quality the core stores."""
        span = 40.0  # dB above sensitivity that counts as perfect
        q = (rx_dbm - self.budget.sensitivity_dbm) / span
        return max(0, min(255, int(q * 255.0)))
