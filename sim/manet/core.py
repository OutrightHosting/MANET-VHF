"""
ctypes binding to the C protocol core.

Standard library only, deliberately: the harness runs anywhere with Python 3 and a C
compiler, with no venv and no pip install. See ADR-0006.

Nothing in this file decides anything. Every protocol behaviour lives in core/ and is
reached through the bridge; Python owns the clock, the radio channel, and the scenario.

State objects are opaque byte buffers whose sizes come from the library itself, so there
is no C struct layout mirrored here to drift out of step.
"""

import ctypes
from pathlib import Path

_ROOT = Path(__file__).resolve().parents[2]


def _load():
    for name in ("libmanetcore.dylib", "libmanetcore.so"):
        p = _ROOT / "build" / name
        if p.exists():
            return ctypes.CDLL(str(p))
    raise RuntimeError(
        "protocol core library not built.\n"
        "  run:  make sim-lib"
    )


_lib = _load()

_P = ctypes.c_void_p
_U8P = ctypes.POINTER(ctypes.c_ubyte)
_ULL = ctypes.c_ulonglong
_UL = ctypes.c_ulong
_U = ctypes.c_uint

_SIGS = {
    "mb_cfg": (ctypes.c_long, [ctypes.c_int]),
    "mb_sizeof": (_UL, [ctypes.c_int]),
    "mb_pdu_set": (None, [_P, _U, _U, _U, _U, _U, _U, _U]),
    "mb_pdu_get": (_U, [_P, ctypes.c_int]),
    "mb_pdu_copy": (None, [_P, _P]),
    "mb_sched_init": (None, [_P]),
    "mb_sched_originate": (ctypes.c_int, [_P, _P, _ULL]),
    "mb_sched_relay": (ctypes.c_int, [_P, _P, _ULL, _U]),
    "mb_sched_take": (ctypes.c_int, [_P, _ULL, _P]),
    "mb_sched_suppress": (ctypes.c_int, [_P, _U, _U]),
    "mb_sched_note_relay": (_UL, [_P, _U, _U, _U]),
    "mb_sched_heard": (_UL, [_P, _U, _U, _U8P, _UL]),
    "mb_mpr_still_needed": (ctypes.c_int, [_P, _U8P, _UL]),
    "mb_sched_depth": (_UL, [_P]),
    "mb_slot_start_us": (_ULL, [_ULL]),
    "mb_slot_is_control": (ctypes.c_int, [_ULL]),
    "mb_slot_next_voice": (_ULL, [_ULL]),
    "mb_nb_init": (None, [_P, _U]),
    "mb_nb_heard": (ctypes.c_int, [_P, _U, _U, _ULL]),
    "mb_nb_advert": (ctypes.c_int, [_P, _U, _U8P, _U8P, _UL, _ULL]),
    "mb_nb_expire": (_UL, [_P, _ULL]),
    "mb_nb_count": (_UL, [_P]),
    "mb_nb_link": (ctypes.c_int, [_P, _U]),
    "mb_nb_should_relay_for": (ctypes.c_int, [_P, _U]),
    "mb_nb_symmetric": (_UL, [_P, _U8P, _UL]),
    "mb_nb_two_hop": (_UL, [_P, _U8P, _UL]),
    "mb_nb_beacon": (_UL, [_P, _U8P, _UL, _U8P, _U8P, _UL]),
    "mb_mpr_select": (_UL, [_P, _P, _U8P, _UL]),
    "mb_mpr_covers_all": (ctypes.c_int, [_P, _P]),
    "mb_mpr_should_relay": (ctypes.c_int, [_P, _U]),
    "mb_dedup_init": (None, [_P]),
    "mb_dedup_check": (ctypes.c_int, [_P, _U, _U, _ULL]),
    "mb_dedup_expire": (_UL, [_P, _ULL, _ULL]),
    "mb_nama_priority": (_UL, [_U, _ULL]),
    "mb_nama_wins": (ctypes.c_int, [_P, _ULL]),
    "mb_nama_next_win": (ctypes.c_int, [_P, _ULL, _U, ctypes.POINTER(_ULL)]),
    "mb_nama_contenders": (_UL, [_P]),
}

for _name, (_res, _args) in _SIGS.items():
    _fn = getattr(_lib, _name)
    _fn.restype = _res
    _fn.argtypes = _args


# --------------------------------------------------------------------- config --

_CFG_NAMES = [
    "slots_per_frame", "slot_us", "frame_us", "guard_us", "burst_us",
    "gross_bitrate", "onair_bits", "header_bits", "max_pdu_bits", "fec_bits",
    "fec_percent", "beacon_bits", "beacon_interval_frames", "nb_hold_slots",
    "max_neighbours", "ttl_max", "voice_bits", "sync_bits",
    "voice_ttl", "heard_max",
]


class Config:
    """What the library was actually compiled with.

    Read from the library rather than assumed, so a harness run against a 3-slot build
    cannot silently report 4-slot numbers.
    """

    def __init__(self):
        for i, name in enumerate(_CFG_NAMES):
            setattr(self, name, int(_lib.mb_cfg(i)))
        self.beacon_interval_slots = self.beacon_interval_frames * self.slots_per_frame

    def __repr__(self):
        return (f"<Config {self.slots_per_frame} x {self.slot_us / 1000:.0f} ms, "
                f"{self.gross_bitrate} bps, {self.onair_bits} on-air bits, "
                f"{self.fec_bits} for FEC ({self.fec_percent}%)>")


CONFIG = Config()


def is_control_slot(slot):
    """Reserved for control traffic. Voice never transmits here."""
    return bool(_lib.mb_slot_is_control(slot))


def next_voice_slot(slot):
    return int(_lib.mb_slot_next_voice(slot))


def _buf(which):
    return ctypes.create_string_buffer(int(_lib.mb_sizeof(which)))


def _u8(n):
    return (ctypes.c_ubyte * max(int(n), 1))()


# ---------------------------------------------------------------- frame types --

VOICE, VOICE_END, BEACON, TC, EMERGENCY, TEXT, POSITION, CONFIG_FRAME = range(8)
PRIO_EMERGENCY, PRIO_VOICE, PRIO_SIGNALLING, PRIO_DATA = range(4)
LINK_NONE, LINK_ASYM, LINK_SYM = range(3)
ADV_ASYM, ADV_SYM, ADV_MPR = range(3)

OK = 0

BROADCAST = 0xFF


class Pdu:
    """One frame, header only — the harness carries no audio."""

    __slots__ = ("buf",)

    def __init__(self, src=0, dst=BROADCAST, type=VOICE, seq=0, ttl=0,
                 prio=PRIO_VOICE, prev=None):
        self.buf = _buf(3)
        if ttl == 0:
            ttl = CONFIG.ttl_max
        if prev is None:
            prev = src
        _lib.mb_pdu_set(self.buf, src, prev, dst, type, seq, ttl, prio)

    def _get(self, i):
        return int(_lib.mb_pdu_get(self.buf, i))

    src = property(lambda self: self._get(0))
    dst = property(lambda self: self._get(1))
    type = property(lambda self: self._get(2))
    seq = property(lambda self: self._get(3))
    ttl = property(lambda self: self._get(4))
    prio = property(lambda self: self._get(5))
    prev = property(lambda self: self._get(6))

    def copy(self):
        other = Pdu.__new__(Pdu)
        other.buf = _buf(3)
        _lib.mb_pdu_copy(other.buf, self.buf)
        return other

    def __repr__(self):
        return (f"<Pdu src={self.src} prev={self.prev} dst={self.dst} "
                f"type={self.type} seq={self.seq} ttl={self.ttl}>")


class Scheduler:
    """core/src/slot.c — what to transmit, and in which slot."""

    __slots__ = ("buf",)

    def __init__(self):
        self.buf = _buf(0)
        _lib.mb_sched_init(self.buf)

    def originate(self, pdu, slot):
        return int(_lib.mb_sched_originate(self.buf, pdu.buf, slot))

    def relay(self, pdu, rx_slot, me):
        return int(_lib.mb_sched_relay(self.buf, pdu.buf, rx_slot, me))

    def take(self, slot):
        out = Pdu()
        if _lib.mb_sched_take(self.buf, slot, out.buf):
            return out
        return None

    def suppress(self, src, seq):
        return bool(_lib.mb_sched_suppress(self.buf, src, seq))

    def note_relay(self, src, seq, frm):
        """Record that `frm` relayed this frame. Does NOT cancel anything."""
        return int(_lib.mb_sched_note_relay(self.buf, src, seq, frm))

    def heard(self, src, seq):
        out = _u8(CONFIG.heard_max)
        n = int(_lib.mb_sched_heard(self.buf, src, seq, out, CONFIG.heard_max))
        return [out[i] for i in range(min(n, CONFIG.heard_max))]

    @property
    def depth(self):
        return int(_lib.mb_sched_depth(self.buf))


class NeighbourTable:
    """core/src/neighbour.c — who I hear, and who hears me."""

    __slots__ = ("buf", "self_addr")

    def __init__(self, self_addr):
        self.buf = _buf(1)
        self.self_addr = self_addr
        _lib.mb_nb_init(self.buf, self_addr)

    def heard(self, frm, quality, slot):
        return int(_lib.mb_nb_heard(self.buf, frm, quality, slot))

    def advert(self, frm, entries, slot):
        n = len(entries)
        addrs, codes = _u8(n), _u8(n)
        for i, (a, c) in enumerate(entries):
            addrs[i], codes[i] = a, c
        return int(_lib.mb_nb_advert(self.buf, frm, addrs, codes, n, slot))

    def expire(self, slot):
        return int(_lib.mb_nb_expire(self.buf, slot))

    def link(self, addr):
        return int(_lib.mb_nb_link(self.buf, addr))

    def should_relay_for(self, frm):
        """Legacy gate: did `frm` explicitly name us as a relay? Needs a fresh beacon."""
        return bool(_lib.mb_nb_should_relay_for(self.buf, frm))

    def still_needed(self, heard):
        """Do we still reach anyone none of `heard` reach?"""
        buf = _u8(len(heard))
        for i, a in enumerate(heard):
            buf[i] = a
        return bool(_lib.mb_mpr_still_needed(self.buf, buf, len(heard)))

    def should_relay(self, frm):
        """Do we reach anyone `frm` does not? Decided from local knowledge alone."""
        return bool(_lib.mb_mpr_should_relay(self.buf, frm))

    @property
    def count(self):
        return int(_lib.mb_nb_count(self.buf))

    def symmetric(self):
        out = _u8(CONFIG.max_neighbours)
        n = int(_lib.mb_nb_symmetric(self.buf, out, CONFIG.max_neighbours))
        return [out[i] for i in range(min(n, CONFIG.max_neighbours))]

    def two_hop(self):
        out = _u8(64)
        n = int(_lib.mb_nb_two_hop(self.buf, out, 64))
        return [out[i] for i in range(min(n, 64))]

    def nama_wins(self, context):
        """Does this radio win the channel-access election for this context?"""
        return bool(_lib.mb_nama_wins(self.buf, context))

    def nama_next_win(self, start, limit=64):
        out = _ULL()
        if _lib.mb_nama_next_win(self.buf, start, limit, ctypes.byref(out)):
            return int(out.value)
        return None

    @property
    def contenders(self):
        return int(_lib.mb_nama_contenders(self.buf))

    def beacon(self, mprs):
        m = _u8(len(mprs))
        for i, a in enumerate(mprs):
            m[i] = a
        cap = CONFIG.max_neighbours
        oa, oc = _u8(cap), _u8(cap)
        n = int(_lib.mb_nb_beacon(self.buf, m, len(mprs), oa, oc, cap))
        return [(oa[i], oc[i]) for i in range(min(n, cap))]


class MprSet:
    """core/src/mpr.c — which neighbours relay for me. Empty means nobody needs to."""

    __slots__ = ("buf", "addrs")

    def __init__(self):
        self.buf = _buf(2)
        self.addrs = []

    def select(self, table):
        cap = CONFIG.max_neighbours
        out = _u8(cap)
        n = int(_lib.mb_mpr_select(table.buf, self.buf, out, cap))
        self.addrs = [out[i] for i in range(min(n, cap))]
        return self.addrs

    def covers_all(self, table):
        return bool(_lib.mb_mpr_covers_all(table.buf, self.buf))


class Dedup:
    """core/src/dedup.c — have I heard this frame before?"""

    __slots__ = ("buf",)

    def __init__(self):
        self.buf = _buf(4)
        _lib.mb_dedup_init(self.buf)

    def check(self, src, seq, slot):
        """True if new. Recording is the side effect."""
        return bool(_lib.mb_dedup_check(self.buf, src, seq, slot))

    def expire(self, slot, age):
        return int(_lib.mb_dedup_expire(self.buf, slot, age))
