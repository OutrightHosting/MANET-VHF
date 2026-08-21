"""
The simulation loop.

Python owns the clock, the radio channel and the scenario. Every protocol decision —
what to transmit, when, whether to relay, who relays for whom — is made by the C core
through sim/manet/core.py. Nothing in this file implements protocol behaviour, and if it
starts to, that is a bug rather than a shortcut (ADR-0006).
"""

from .core import (
    BEACON, VOICE, PRIO_SIGNALLING, PRIO_VOICE, BROADCAST, CONFIG,
    Dedup, MprSet, NeighbourTable, Pdu, Scheduler,
)
from .mobility import Static
from .radio import Channel


class Node:
    def __init__(self, index, addr, pos):
        self.index = index
        self.addr = addr
        self.pos = pos

        self.sched = Scheduler()
        self.nb = NeighbourTable(addr)
        self.mpr = MprSet()
        self.dedup = Dedup()

        self.seq = 0
        self.beacon_entries = []

        # Slot phases this radio needs for voice: the one it transmits in, and the one
        # it must listen in. It may not beacon in either.
        self.voice_tx_phase = None
        self.voice_rx_phase = None

        # metrics
        self.heard_payloads = {}   # (src, seq) -> slot first decoded
        self.relayed = 0
        self.originated = 0
        self.beacons_sent = 0
        self.decode_failures = 0


class Simulation:
    def __init__(self, positions, env, budget=None, talker=0):
        self.cfg = CONFIG
        self.channel = Channel(env, budget)
        # `positions` may be a fixed list or a Mobility. Nodes carry a current position
        # that is refreshed once per frame — at walking pace a frame is 7 cm, so there is
        # nothing to gain from updating per slot.
        self.mobility = positions if hasattr(positions, "positions_at") else Static(positions)
        start = self.mobility.positions_at(0)
        self.nodes = [Node(i, i + 1, p) for i, p in enumerate(start)]
        self.talker = talker
        self.slot = 0
        self.tx_log = []
        self.collisions = 0

        # Payload identity for measurement only.
        #
        # The wire sequence number is 8 bits and wraps every 256 payloads — 15.4 s of
        # continuous talking at one payload per 60 ms frame. That is correct on the air
        # (duplicate suppression ages entries out long before), but a metric keyed on
        # (src, seq) silently merges payload 0 with payload 256 and reports the loss as
        # a delivery failure. So originations are logged with a monotonic id and
        # receipts are attributed to the most recent origination at or before them.
        self.origin_log = {}   # (src, seq) -> [slots, ascending]
        self.origin_count = 0

    # ------------------------------------------------------------------ helpers --

    @property
    def positions(self):
        return [n.pos for n in self.nodes]

    def _beacon_slot_for(self, node):
        """
        Which slot in the beacon interval this radio beacons in.

        Two constraints, both learned the hard way in Phase 0.

        First, the stagger step must not be a multiple of the slots per frame. If it is,
        every radio's beacon lands on the same phase of the voice cycle and the whole
        group interferes with the same thing.

        Second — and this is the one the brief does not anticipate — a radio must not
        beacon in a slot phase it needs for voice. There are two such phases: the one it
        transmits voice in, and the one it must listen in to hear its upstream
        neighbour. Beacon in the first and two of its own transmissions collide in the
        air; beacon in the second and it goes deaf in exactly the slot that matters.

        This is a channel-access rule and it belongs in the MAC, not in a simulator. It
        lives here for now because the core has no channel access mechanism at all —
        see OQ-0009, which this makes concrete.
        """
        interval = self.cfg.beacon_interval_slots
        spf = self.cfg.slots_per_frame
        n = max(len(self.nodes), 1)

        step = max(interval // n, 1)
        if step % spf == 0:
            step += 1
        base = (node.index * step) % interval

        busy = {p for p in (node.voice_tx_phase, node.voice_rx_phase) if p is not None}
        for shift in range(spf):
            candidate = (base + shift) % interval
            if candidate % spf not in busy:
                return candidate
        return base

    # -------------------------------------------------------------------- phases --

    def _schedule_beacons(self, slot):
        interval = self.cfg.beacon_interval_slots
        for n in self.nodes:
            if slot % interval != self._beacon_slot_for(n):
                continue
            n.mpr.select(n.nb)
            n.beacon_entries = n.nb.beacon(n.mpr.addrs)
            pdu = Pdu(src=n.addr, prev=n.addr, dst=BROADCAST, type=BEACON,
                      seq=n.seq & 0xFF, prio=PRIO_SIGNALLING, ttl=1)
            n.seq = (n.seq + 1) & 0xFF
            if n.sched.originate(pdu, slot) == 0:
                n.beacons_sent += 1

    def _schedule_voice(self, slot, active):
        if not active:
            return
        # One Codec2 payload per frame, so a new burst begins at slot 0 of each frame.
        if slot % self.cfg.slots_per_frame != 0:
            return
        n = self.nodes[self.talker]
        pdu = Pdu(src=n.addr, prev=n.addr, dst=0xC0, type=VOICE,
                  seq=n.seq & 0xFF, prio=PRIO_VOICE)
        n.seq = (n.seq + 1) & 0xFF
        if n.sched.originate(pdu, slot) == 0:
            n.originated += 1
            self.origin_log.setdefault((pdu.src, pdu.seq), []).append(slot)
            self.origin_count += 1
            n.heard_payloads.setdefault(self._payload_id(pdu.src, pdu.seq, slot), slot)

    def _collect(self, slot):
        out = []
        _ = slot
        for n in self.nodes:
            pdu = n.sched.take(slot)
            if pdu is not None:
                if pdu.type in (VOICE,):
                    n.voice_tx_phase = slot % self.cfg.slots_per_frame
                out.append((n.index, (pdu, n.beacon_entries if pdu.type == BEACON else None)))
        return out

    def _deliver(self, slot, txs):
        if not txs:
            return
        transmitting = {i for i, _ in txs}
        positions = self.positions

        for rx in self.nodes:
            # Half duplex: a radio cannot hear anything while its own PA is keyed.
            if rx.index in transmitting:
                continue

            got = self.channel.decode(rx.pos, txs, positions)
            if got is None:
                # Something was in the air but nothing came out of the demodulator.
                if any(self.channel.rx_dbm(_dist(rx.pos, positions[i])) >=
                       self.channel.budget.sensitivity_dbm for i, _ in txs):
                    rx.decode_failures += 1
                    self.collisions += 1
                continue

            tx_index, (pdu, entries), dbm = got
            self._receive(rx, pdu, entries, dbm, slot)

    def _receive(self, rx, pdu, entries, dbm, slot):
        quality = self.channel.quality(dbm)

        # The link is to whoever actually transmitted this copy — the previous hop, not
        # the origin. Getting this wrong builds a neighbour table full of strangers.
        rx.nb.heard(pdu.prev, quality, slot)

        if pdu.type == BEACON:
            if entries:
                rx.nb.advert(pdu.prev, entries, slot)
            return

        rx.voice_rx_phase = slot % self.cfg.slots_per_frame

        if not rx.dedup.check(pdu.src, pdu.seq, slot):
            # An echo. Someone else already relayed it, so ours is redundant —
            # passive acknowledgement.
            rx.sched.suppress(pdu.src, pdu.seq)
            return

        pid = self._payload_id(pdu.src, pdu.seq, slot)
        if pid is not None:
            rx.heard_payloads.setdefault(pid, slot)

        # The forwarding rule: relay only for a neighbour that selected us.
        if rx.nb.should_relay_for(pdu.prev):
            if rx.sched.relay(pdu, slot, rx.addr) == 0:
                rx.relayed += 1

    def _payload_id(self, src, seq, slot):
        """Which origination a receipt at `slot` belongs to, disambiguating seq wrap."""
        slots = self.origin_log.get((src, seq))
        if not slots:
            return None
        best = None
        for t in slots:
            if t <= slot:
                best = t
            else:
                break
        return None if best is None else (src, seq, best)

    def _move(self, slot):
        if slot % self.cfg.slots_per_frame:
            return
        pos = self.mobility.positions_at(slot * self.cfg.slot_us)
        for n, p in zip(self.nodes, pos):
            n.pos = p

    def _housekeep(self, slot):
        if slot % self.cfg.slots_per_frame:
            return
        for n in self.nodes:
            n.nb.expire(slot)
            n.dedup.expire(slot, self.cfg.beacon_interval_slots)

    # ----------------------------------------------------------------------- run --

    def run(self, slots, voice_from=None, voice_to=None):
        # voice_from/voice_to are ABSOLUTE slot numbers, not offsets into this call —
        # runs are chained, so a count would silently mean the wrong thing on the
        # second call.
        end = self.slot + slots
        if voice_to is None:
            voice_to = end
        for _ in range(slots):
            slot = self.slot
            active = (voice_from is not None and voice_from <= slot < voice_to)
            self._move(slot)
            self._schedule_beacons(slot)
            self._schedule_voice(slot, active)
            txs = self._collect(slot)
            if txs:
                self.tx_log.append((slot, [(i, p[0].src, p[0].seq, p[0].type) for i, p in txs]))
            self._deliver(slot, txs)
            self._housekeep(slot)
            self.slot += 1
        return self

    # ------------------------------------------------------------------- metrics --

    def converged(self):
        """Every node has selected relays and they cover its two-hop neighbourhood."""
        for n in self.nodes:
            n.mpr.select(n.nb)
            if not n.mpr.covers_all(n.nb):
                return False
        return True

    def relaying_now(self):
        """Total relays currently selected across the group. Zero means nothing relays."""
        return sum(len(n.mpr.select(n.nb)) for n in self.nodes)

    def reachable_from(self, index):
        """How many nodes are reachable from `index` through symmetric links."""
        seen = {self.nodes[index].addr}
        frontier = [self.nodes[index]]
        by_addr = {n.addr: n for n in self.nodes}
        while frontier:
            nxt = []
            for n in frontier:
                for a in n.nb.symmetric():
                    if a not in seen and a in by_addr:
                        seen.add(a)
                        nxt.append(by_addr[a])
            frontier = nxt
        return len(seen)

    def relay_total(self):
        return sum(n.relayed for n in self.nodes)

    def delivery(self, src_addr):
        """Fraction of the talker's payloads each node actually received."""
        sent = self.nodes[self.talker].originated
        if sent == 0:
            return {}
        out = {}
        for n in self.nodes:
            got = sum(1 for pid in n.heard_payloads if pid[0] == src_addr)
            out[n.index] = got / sent
        return out

    def latency_slots(self, src_addr):
        """Slots from origination to first decode, per node."""
        origin = self.nodes[self.talker]
        first = {k: v for k, v in origin.heard_payloads.items() if k[0] == src_addr}
        # k is (src, seq, origination_slot)
        out = {}
        for n in self.nodes:
            deltas = [n.heard_payloads[k] - first[k]
                      for k in first if k in n.heard_payloads]
            if deltas:
                out[n.index] = sum(deltas) / len(deltas)
        return out


def _dist(a, b):
    return ((a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2) ** 0.5
