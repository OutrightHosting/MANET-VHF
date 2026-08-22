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

from .terrain import ANTENNA_HEIGHT_M, Flat, diffraction_db

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
    # Log-normal shadowing, one standard deviation.
    #
    # NOT copied from anyone: derived from FFI's own published quantiles for NBWF mode N1
    # (22.0 km median, 13.1 km at the 90% level, 36.9 km at the 10% level, under Egli with
    # exponent 4). Both sides independently give 7.0 dB, which is the check that it is a
    # real parameter rather than a ratio somebody rounded, and it sits squarely in the
    # published range for VHF land-mobile shadowing.
    #
    # Used for REPORTING only -- see usable_range_quantiles. The channel model itself is
    # still deterministic, so a given pair of positions always produces the same result.
    # Making it stochastic per link is a larger change and a separate question.
    shadowing_db: float = 7.0

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


def egli_path_loss_db(d_m, freq_hz, h_tx_m=1.5, h_rx_m=1.5):
    """
    Egli (1957), median path loss over irregular terrain below 1 GHz:

        L50 = 20 log10(f_MHz) + 40 log10(d_km) - 20 log10(h_tx * h_rx) + 76.3

    Here as an INDEPENDENT CROSS-CHECK on `Environment.path_loss_db`, not as a replacement.
    FFI used Egli for NBWF after discarding the NBWF physical-layer draft's own model as
    giving exaggerated ranges, so it is the natural second opinion (see nbwf-lessons.md).

    Note what transplanting FFI's "exponent 4" alone would get wrong: Egli's intercept and
    its antenna-height term are both different from a free-space-intercept log-distance
    model. Setting exponent=4.0 in Environment gives 568 m where Egli gives 3967 m for the
    same radio. The exponent does not travel on its own.
    """
    f_mhz = freq_hz / 1e6
    d_km = max(float(d_m), 1.0) / 1000.0
    return (20.0 * math.log10(f_mhz) + 40.0 * math.log10(d_km)
            - 20.0 * math.log10(h_tx_m * h_rx_m) + 76.3)


def egli_range_m(budget, h_tx_m=1.5, h_rx_m=1.5, margin_db=0.0):
    """Range at which Egli's median loss reaches the budget. `margin_db` subtracts fade
       margin -- FFI's published 22 km implies about 12 dB of it that their table does not
       itemise."""
    limit = budget.max_path_loss_db - margin_db
    lo, hi = 1.0, 300000.0
    for _ in range(80):
        mid = (lo + hi) / 2.0
        if egli_path_loss_db(mid, budget.freq_hz, h_tx_m, h_rx_m) < limit:
            lo = mid
        else:
            hi = mid
    return lo


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


def usable_range_quantiles(env, budget):
    """
    Range as a distribution, which is the only honest way to quote it.

    A single number implies a certainty the physics does not have. Two radios the same
    distance apart, in the same woodland, differ by several dB depending on what happens
    to be between them -- so the useful statement is "half the links reach this far, nine
    in ten reach at least this far".

    Returns (p90, median, p10): the conservative figure, the median, and the optimistic
    one. Note the ordering -- the 90% QUANTILE OF LINKS is the SHORTEST range, because it
    is the distance nine links in ten will manage.
    """
    z = 1.2816   # normal quantile at 10% / 90%
    med = usable_range_m(env, budget)
    lo = usable_range_m(env, _with_margin(budget, +z * budget.shadowing_db))
    hi = usable_range_m(env, _with_margin(budget, -z * budget.shadowing_db))
    return lo, med, hi


def _with_margin(budget, extra_loss_db):
    """A copy of the budget with `extra_loss_db` of additional path loss to overcome."""
    import dataclasses
    return dataclasses.replace(
        budget, sensitivity_dbm=budget.sensitivity_dbm + extra_loss_db)


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

    # ON. Identical copies transmitted in the same slot are excluded from each other's
    # interference sum rather than colliding — the barrage premise (ADR-0011), and the
    # single largest unverified assumption in the project. It is what buys one hop per slot
    # instead of 6.32, and B-15 confirmed the flood never jams itself: zero voice-against-
    # voice collisions at every density measured. The bench question is whether real
    # hardware behaves this way at our symbol rate — OQ-0028, and it is not settled.
    # Note this claims NO combining gain: the strongest copy is decoded and the others are
    # merely not counted against it, so N co-located radios receive what one receives.
    CONCURRENT_IDENTICAL = True

    def rx_dbm(self, distance_m):
        """Received power over flat ground. Kept for probes and the budget tool."""
        key = round(distance_m, 1)
        loss = self._loss_cache.get(key)
        if loss is None:
            loss = self.env.path_loss_db(key, self.budget.freq_hz)
            self._loss_cache[key] = loss
        return self.budget.eirp_dbm + self.budget.rx_offset_db - loss

    # Log-normal shadowing applied PER LINK rather than only in reporting. Off by default
    # because it changes every delivery figure in the project; see SHADOW_GRID_M.
    SHADOWING = False

    # Decorrelation distance for the SHARED half of the fade. Shadowing is caused by what is
    # physically between two radios -- a thicket, a dip, a wall -- so it is stable while they
    # stand still and changes as they walk. Quantising position to this grid gives exactly
    # that: the same pair in the same place always gets the same answer, and moving a grid
    # step gets a new one. 100 m is mid-range for the published figures on land-mobile
    # shadowing.
    SHADOW_GRID_M = 100.0

    # How the total sigma splits between the two halves below. Variances add, so each half
    # carries sigma * sqrt(SHADOW_SHARED_FRACTION) and sigma * sqrt(1 - it), and the total is
    # unchanged at LinkBudget.shadowing_db. 0.5 is an even split and is a MODELLING CHOICE,
    # not a measured value -- the literature reports correlation coefficients between about
    # 0.3 and 0.8 depending on environment and antenna height, and 0.5 sits in the middle of
    # that. See OQ-0036.
    SHADOW_SHARED_FRACTION = 0.5

    @staticmethod
    def _gauss(*words):
        """A standard normal deviate from a stable hash of the given integers."""
        h = 0x811C9DC5
        for v in words:
            h = ((h ^ (v & 0xFFFFFFFF)) * 0x01000193) & 0xFFFFFFFF
            h ^= h >> 15
        # Box-Muller from two decorrelated words of the hash.
        u1 = ((h & 0xFFFF) + 1) / 65537.0
        h2 = (h * 0x9E3779B1) & 0xFFFFFFFF
        u2 = (h2 >> 16) / 65536.0
        return math.sqrt(-2.0 * math.log(u1)) * math.cos(2.0 * math.pi * u2)

    def _shadow_db(self, a, b):
        """
        A stable pseudo-random fade for this pair, in this place, in TWO parts.

        Deterministic, so runs stay reproducible with no seed threaded through the
        simulator, and SYMMETRIC, so a link is the same measured from either end -- a
        radio that can hear you must be a radio you can hear.

        SHARED: quantised to SHADOW_GRID_M. Two radios standing near each other really do
        look through the same stand of trees at the same hillside, so the obstruction they
        meet is largely the same obstruction.

        LOCAL: keyed on the exact positions, so every link gets its own draw. This is the
        tree one person happens to be beside, the dip they are standing in, which way their
        body is turned. It belongs to that pair and nobody else.

        THE LOCAL HALF IS WHY THIS IS NOT ONE HASH. With only the shared term, every radio
        in a group sits in a single grid cell and all sixteen links between two groups of
        four take ONE roll of the dice -- measured, and a bad roll severed all sixteen at
        once, taking a scale scenario from 32/32 radios in the conversation to 4/32. Real
        groups do not fail that way: a huddle of four has four different sets of local
        clutter, and that diversity is most of what makes standing together useful.
        """
        g = self.SHADOW_GRID_M
        pa = (int(a[0] // g), int(a[1] // g))
        pb = (int(b[0] // g), int(b[1] // g))
        lo, hi = (pa, pb) if pa <= pb else (pb, pa)
        shared = self._gauss(lo[0], lo[1], hi[0], hi[1], 0x5EED)

        # Decimetre resolution: fine enough that co-located radios differ, coarse enough
        # that floating-point noise cannot change the answer.
        qa = (int(round(a[0] * 10.0)), int(round(a[1] * 10.0)))
        qb = (int(round(b[0] * 10.0)), int(round(b[1] * 10.0)))
        la, lb = (qa, qb) if qa <= qb else (qb, qa)
        local = self._gauss(la[0], la[1], lb[0], lb[1], 0xC10D)

        f = self.SHADOW_SHARED_FRACTION
        return self.budget.shadowing_db * (
            math.sqrt(f) * shared + math.sqrt(1.0 - f) * local)

    # Effective earth radius. Refraction bends VHF slightly downward, so the radio horizon
    # is further than the optical one -- the standard 4/3 approximation.
    EARTH_K = 4.0 / 3.0
    EARTH_RADIUS_M = 6371000.0

    def _antenna_heights(self, a, b):
        """Height of each antenna above sea level: ground under it, plus the person."""
        return (self.terrain.height(a[0], a[1]) + ANTENNA_HEIGHT_M,
                self.terrain.height(b[0], b[1]) + ANTENNA_HEIGHT_M)

    def height_gain_db(self, a, b):
        """
        What standing somewhere high is worth.

        Egli's median-loss model carries the antenna-height dependence explicitly, as
        -20 log10(h_tx * h_rx). Environment.path_loss_db does not: it is a log-distance fit
        with a vegetation term, calibrated for two radios on foot, and it gives every radio
        the same range whatever it is standing on. That is wrong in the one situation the
        product exists for. terrain.py's own docstring describes it exactly -- "Both can hear
        anyone standing on the ridge between them. That is a repeater on a hill, except
        nobody sited it" -- and until now the model could represent the hill only as an
        obstruction, never as a vantage point.

        So this applies EGLI'S HEIGHT TERM AS A DELTA from the 1.5 m both-on-foot case the
        distance model is already calibrated for. Two radios on the flat get exactly 0 dB and
        nothing changes; a radio on an 80 m ridge gets about 35 dB, which in a 40 log10(d)
        model is a little over seven times the range.

        Height is worth far more than power here. The same 35 dB from the PA would be about
        3 kW.
        """
        ha, hb = self._antenna_heights(a, b)
        ref = ANTENNA_HEIGHT_M * ANTENNA_HEIGHT_M
        return 20.0 * math.log10((ha * hb) / ref)

    def los_limit_m(self, a, b):
        """
        Distance at which the earth's curvature puts the far radio below the horizon.

        Egli has no line-of-sight cap and will happily report 215 km for two radios on the
        same ridge, which is nonsense -- they are 74 km apart over the bulge by then. The
        gain above is only real out to here.
        """
        ha, hb = self._antenna_heights(a, b)
        k = 2.0 * self.EARTH_K * self.EARTH_RADIUS_M
        return math.sqrt(k * ha) + math.sqrt(k * hb)

    def rx_dbm_between(self, a, b):
        """Received power between two points, including terrain and antenna height."""
        d = math.hypot(a[0] - b[0], a[1] - b[1])
        p = self.rx_dbm(d)
        if not isinstance(self.terrain, Flat):
            p -= diffraction_db(a, b, self.terrain, self.budget.freq_hz)
            p += self.height_gain_db(a, b)
            if d > self.los_limit_m(a, b):
                # Beyond the horizon the height gain is not available at all. Charged as a
                # hard floor rather than modelled -- over-the-horizon diffraction is a real
                # mechanism and this is not it, but nothing in this project should be
                # relying on a path the far radio cannot see.
                return -999.0
        if self.SHADOWING:
            p -= self._shadow_db(a, b)
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
            # A SIGNAL THE RECEIVER CANNOT DETECT CANNOT JAM IT.
            #
            # This sum previously ran over every other transmission in the network,
            # including ones tens of dB below the demodulator's floor. In a twelve-radio
            # chain there are one or two of them and it makes no difference. In a
            # hundred-radio mesh there is a median of EIGHTEEN per slot, and they
            # contributed a median of 100% of the interference power -- so a receiver was
            # being jammed entirely by transmissions it could not hear.
            #
            # The effect was to lose half of everything the talker said at the FIRST hop
            # while hops two to seven ran at 96-99%, and to make the network worse the more
            # radios were added to it: 40 -> 200 radios on the same ground plateaued near
            # 50% delivery. Both are gone once the floor is applied.
            #
            # Sub-floor signals are not ignored on principle -- they are already inside the
            # sensitivity figure, which is defined against the receiver's own noise. Adding
            # them again on top counts the same noise twice.
            others = [p for p in others if p[0] >= self.budget.sensitivity_dbm]
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
