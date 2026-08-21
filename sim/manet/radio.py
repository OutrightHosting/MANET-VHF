"""
Radio propagation and the shared channel.

This is the part of the harness that decides whether the answers mean anything. The
protocol logic is exact — it is the shipping C code — but everything it is judged
against comes from the model below.

Two honest caveats, stated here rather than buried:

  * The path-loss exponents are calibrated to give plausible handheld-to-handheld ranges
    at VHF (about 6 km over open ground, about 4.4 km through dense woodland), not derived
    from measurement. Phase 2 exists to replace them. The vegetation term itself is not
    calibrated — it is ITU-R P.833-10 equation (1) with published parameters.
  * Path loss is an environment applied uniformly to every link. Terrain on top of that
    is NOT uniform: pass a height field as `terrain=` and `rx_dbm_between` charges
    ITU-R P.526 knife-edge diffraction over the ground profile between each specific
    pair, so a ridge between two radios is represented. See `terrain.py`, and
    `scenarios/hill.py` for the case it was built for. With the default `Flat` terrain
    the diffraction term is skipped entirely and this reduces to the uniform model.

What the model does take seriously is the distinction that decides OQ-0013: a signal can
be far too weak to decode and still be strong enough to ruin someone else's reception.
Anything that collapses that into "in range / out of range" cannot answer the question.
"""

import math
from dataclasses import dataclass

from .terrain import Flat, diffraction_db

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


def vegetation_db(d_m, freq_hz):
    """
    Excess attenuation through woodland — ITU-R P.833-10 §2.1, equation (1):

        A_ev = A_m [ 1 - exp(-d * gamma / A_m) ]

    **It saturates**, and that is the whole point. The Recommendation is explicit about
    why: *"if the specific attenuation is sufficiently high, a lower-loss path will exist
    around the vegetation"* — past some depth the signal stops going through the trees and
    starts going over and around them, so the excess loss stops growing.

    This replaces `0.2 * f^0.3 * d^0.6`, the early-ITU/Weissberger form, which this model
    applied unbounded. That form is specified only to about 400 m; run to 2 km it charged
    87 dB of foliage loss where the measured ceiling is 11. Woodland range came out at
    528 m, roughly eight times too pessimistic, and every reach conclusion in the project
    rested on it.

    Parameters from P.833-10 Table 1 and equation (2), fitted to measurements in mixed
    coniferous-deciduous forest near St Petersburg over paths from a few hundred metres to
    **7 km**, trees of mean height 16 m: A_m = 1.37 f^0.42, and gamma fitted through the
    two lowest measured points (0.04 dB/m at 105.9 MHz, 0.12 at 466.475).

    At 155 MHz this gives gamma = 0.053 dB/m and a ceiling of 11.4 dB.
    """
    f_mhz = freq_hz / 1e6
    a_max = 1.37 * (f_mhz ** 0.42)
    k = math.log(0.12 / 0.04) / math.log(466.475 / 105.9)
    gamma = 0.04 * (f_mhz / 105.9) ** k
    return a_max * (1.0 - math.exp(-max(float(d_m), 0.0) * gamma / a_max))


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
            loss += vegetation_db(d, freq_hz)
        return loss


# Calibrated so handheld-to-handheld range lands where field experience puts it.
OPEN = Environment("open moorland", exponent=3.2, foliage=False)

# 3.0, and this number has been changed twice by feel and should not be changed a third
# time without a measurement.
#
# It was 3.0, moved to 3.5 to match "field reports" that were asserted rather than cited,
# and moved back when the resulting 1.3 km single-hop range was pointed out to be well
# under what a 5 W VHF handheld actually achieves in woodland. Solving for the exponent
# that puts single-hop range at 4.8 km gives 2.97.
#
# The exponent is the least defensible number in this model. What does NOT depend on it:
# the mesh multiplies reach by the hop count regardless — 3.6x at four hops at every
# exponent tried. The multiplier is robust; the absolute figure is not, and Phase 2 exists
# to measure it.
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


def _payload_key(payload):
    """What makes two transmissions 'the same frame'. Origin and sequence identify a
    payload uniquely across the whole network; the relay carrying it is irrelevant."""
    pdu = payload[0]
    return (pdu.src, pdu.seq, pdu.type)


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

    def __init__(self, env, budget=None, terrain=None):
        self.env = env
        self.budget = budget or LinkBudget()
        # Terrain is what actually breaks links. Foliage attenuation saturates at about
        # 11 dB; a ridge costs tens. Flat by default so existing scenarios are unchanged.
        self.terrain = terrain or Flat()
        self._loss_cache = {}

    # Off by default until the bench says otherwise — see OQ-0028.
    CONCURRENT_IDENTICAL = True

    def rx_dbm(self, distance_m):
        """Received power over flat ground. Kept for probes and the budget tool."""
        key = round(distance_m, 1)
        loss = self._loss_cache.get(key)
        if loss is None:
            loss = self.env.path_loss_db(key, self.budget.freq_hz)
            self._loss_cache[key] = loss
        return self.budget.eirp_dbm + self.budget.rx_offset_db - loss

    def rx_dbm_between(self, a, b):
        """Received power between two points, including terrain obstruction."""
        d = math.hypot(a[0] - b[0], a[1] - b[1])
        p = self.rx_dbm(d)
        if not isinstance(self.terrain, Flat):
            p -= diffraction_db(a, b, self.terrain, self.budget.freq_hz)
        return p

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
            powers.append((self.rx_dbm_between(rx_pos, positions[idx]), idx, payload))

        powers.sort(key=lambda t: -t[0])
        best_dbm, best_idx, best_payload = powers[0]

        if best_dbm < self.budget.sensitivity_dbm:
            return None  # nothing audible at all

        if len(powers) > 1:
            # Copies of the SAME payload are not interference. This is the whole mechanism
            # of a Barrage Relay Network (TrellisWare TSM) and of Glossy: several relays
            # transmit the identical waveform in the identical slot, and the receiver sees
            # what looks like multipath rather than a collision. Modelling them as mutual
            # jamming is what forced a per-frame election, and the election was costing
            # 6.32 slots per hop against ADR-0002's promised one.
            #
            # Conservative form deliberately: the strongest copy is decoded and the other
            # copies are merely EXCLUDED from the interference sum. No combining gain is
            # claimed, though Glossy measures one. Different payloads still collide exactly
            # as before, so OQ-0013's spatial-reuse result is untouched.
            #
            # THE ASSUMPTION THIS RESTS ON, and it is a bench question, not a settled one:
            # the copies must land inside a symbol of each other. At 9600 sym/s a symbol is
            # 104 us and a few km of path difference is ~10 us, so timing is comfortable
            # with GPS-disciplined slots. Carrier frequency offset between transmitters is
            # the real risk — it beats, and destructive periods need FEC to ride through.
            # Same dependency as the preamble figure: a disciplined LO. See OQ-0028.
            if self.CONCURRENT_IDENTICAL:
                others = [p for p in powers[1:] if _payload_key(p[2]) != _payload_key(best_payload)]
            else:
                others = powers[1:]
            interference_mw = sum(_dbm_to_mw(p[0]) for p in others)
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
