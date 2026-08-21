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
        # Every phase on which this radio has recently heard voice. It must keep its ears
        # free on all of them, not only the most recent, or a radio carrying one
        # conversation goes deaf to a second.
        self.rx_phases = {}
        # Which ORIGIN each receive phase carried. Hearing several neighbours is normal
        # and not a reason to protect ears; hearing two different conversations is.
        self.rx_sources = {}

        # Channel access state. A radio that can hear a stream in progress holds its
        # beacon rather than keying up over it.
        self.last_voice_slot = None
        self.beacon_deferrals = 0
        self.last_beacon = None

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

    # Beacon slots are drawn from a small pool and REUSED across the network. Two
    # radios far enough apart can beacon in the same slot for the same reason two
    # radios far apart can relay in the same slot: neither can hear the other.
    #
    # This is what makes the mesh scale. A globally unique beacon slot per radio makes
    # control overhead grow linearly with the size of the whole network, which is
    # nonsense for traffic that never travels more than one hop. With reuse, the cost
    # depends on how many radios are within earshot of each other — local density — and
    # not at all on how many exist.
    BEACON_POOL = 32

    def _beacon_slot_for(self, node):
        """
        Deprecated. Beacon slots are no longer hashed from an address — see
        _schedule_beacons, which elects them with NAMA. Retained only so older probes do
        not break.
        """
        interval = self.cfg.beacon_interval_slots
        return (node.index * max(interval // max(len(self.nodes), 1), 1)) % interval

    # -------------------------------------------------------------------- phases --

    # How long after hearing voice a radio treats the channel as occupied, and how many
    # beacons it may hold before sending one anyway. Two frames is enough to cover a
    # relay chain passing through; six deferrals is ~12 s, inside the neighbour hold
    # time, so a held beacon can never age a link out.
    QUIET_FRAMES = 2
    MAX_DEFERRALS = 6
    # Off. It buys a few points of delivery in dense topologies and costs three slots
    # per hop — at 50 ms slots that is 150 ms against 50 ms, and the latency budget
    # cannot pay it. Coverage-based suppression now handles what the stagger was for.
    STAGGER = False

    def _schedule_beacons(self, slot):
        """
        Beacons go out in slots this radio WINS, not slots it hashed itself into.

        The election is NAMA (Bao & Garcia-Luna-Aceves, MobiCom 2001): every radio
        computes a pseudo-random priority for itself and for everything within two hops,
        and transmits only if its own is highest. All of them derive the same numbers from
        the same inputs, so the outcome is collision-free by construction with no
        handshake and nothing to go stale.

        What this replaces was hashing an address into a slot pool with no way to detect a
        collision. Two radios that hashed alike never heard each other and were invisible
        to every neighbour in common, permanently and silently — about a 39% chance across
        twelve radios in a 132-slot interval.

        Two-hop contention is the point. A radio cannot hear a collision it causes two hops
        away, and on a relaying mesh the victim is the shared relay between them.
        """
        interval = self.cfg.beacon_interval_slots
        for n in self.nodes:
            # Due for a beacon?
            if n.last_beacon is not None and (slot - n.last_beacon) < interval:
                continue
            # Win this slot?
            if not n.nb.nama_wins(slot):
                continue
            # Winning the election says nobody within two hops will transmit in this slot.
            # It says nothing about whether THIS radio can afford to. Two local rules still
            # apply, and dropping them when NAMA went in cost eight points of delivery
            # under movement:
            #
            #  - do not beacon in a phase this radio needs to hear voice on, or it goes
            #    deaf in the slot that matters;
            #  - hold off while a transmission is passing through, bounded so neighbours
            #    can never age this radio out mid-call.
            if (n.voice_rx_phase is not None
                    and slot % self.cfg.slots_per_frame == n.voice_rx_phase):
                continue

            quiet = self.QUIET_FRAMES * self.cfg.slots_per_frame
            busy = (n.last_voice_slot is not None
                    and (slot - n.last_voice_slot) < quiet)
            if busy and n.beacon_deferrals < self.MAX_DEFERRALS:
                n.beacon_deferrals += 1
                continue
            n.beacon_deferrals = 0

            n.mpr.select(n.nb)
            n.beacon_entries = n.nb.beacon(n.mpr.addrs)
            pdu = Pdu(src=n.addr, prev=n.addr, dst=BROADCAST, type=BEACON,
                      seq=n.seq & 0xFF, prio=PRIO_SIGNALLING, ttl=1)
            n.seq = (n.seq + 1) & 0xFF
            if n.sched.originate(pdu, slot) == 0:
                n.beacons_sent += 1
                n.last_beacon = slot

    def voice_phase(self, node):
        """
        Which slot of the frame this radio begins a transmission in.

        Not a NAMA election. Voice needs a payload every frame — one slot in four — while
        an election in an eleven-radio neighbourhood yields a win roughly one slot in
        twelve. Channel-access election belongs on a superframe above the voice frame, not
        inside it; that is the sizing constraint the literature review identifies and the
        reason earlier attempts to reserve control slots inside the frame all failed.

        So this stays a hash of the radio's own address. Note a plain odd multiplier does
        not work: times five modulo four is just modulo four, and every odd address lands
        on the same two phases.
        """
        h = (node.addr * 0x9E3779B1) & 0xFFFFFFFF
        return (h >> 16) % self.cfg.slots_per_frame

    def _schedule_voice(self, slot, active):
        if not active:
            return
        n = self.nodes[self.talker]
        # One Codec2 payload per frame, beginning at this radio's own phase.
        if slot % self.cfg.slots_per_frame != self.voice_phase(n):
            return
        # Voice TTL is bounded by the mouth-to-ear budget, not by the field width.
        pdu = Pdu(src=n.addr, prev=n.addr, dst=0xC0, type=VOICE,
                  seq=n.seq & 0xFF, prio=PRIO_VOICE, ttl=self.cfg.voice_ttl)
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
                    n.last_voice_slot = slot
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
        rx.rx_phases[rx.voice_rx_phase] = slot
        rx.rx_sources[rx.voice_rx_phase] = pdu.src
        rx.last_voice_slot = slot

        if not rx.dedup.check(pdu.src, pdu.seq, slot):
            # An echo — somebody relayed it. That is not on its own a reason to stay
            # quiet. Record who, then ask whether our own transmission would still reach
            # anyone none of them reach; cancel only if it would not.
            #
            # Cancelling on the first duplicate is a counter-based scheme at C=1, which
            # cannot guarantee every radio is reached (Ni et al., MobiCom'99). Two radios
            # can both hear a relay that covers neither's far side, both fall silent, and
            # the frame stops with nothing to indicate it.
            rx.sched.note_relay(pdu.src, pdu.seq, pdu.prev)
            heard = rx.sched.heard(pdu.src, pdu.seq)
            if heard and not rx.nb.still_needed(heard):
                rx.sched.suppress(pdu.src, pdu.seq)
            return

        pid = self._payload_id(pdu.src, pdu.seq, slot)
        if pid is not None:
            rx.heard_payloads.setdefault(pid, slot)

        # The forwarding rule: relay if we reach somebody the sender does not. Decided
        # from local knowledge, so it survives beacons being unable to get through.
        #
        # Receiver-decided relaying has one weakness that sender-decided MPR does not:
        # every candidate reaches the same conclusion in the same slot and they all fire
        # together, so passive acknowledgement cannot help — nobody goes first. Staggering
        # them by link quality fixes that and buys redundancy at the same time. The radio
        # with the strongest link to the sender relays immediately; weaker candidates take
        # the following slots, and cancel themselves the moment they hear the frame go on
        # without them. If the best relay fails, the next one covers a slot later.
        if rx.nb.should_relay(pdu.prev):
            # Stagger candidates by link quality so they do not all fire in the same
            # slot. Sized as a FRACTION of the frame, not a fixed slot count: it was
            # tuned at 15 ms slots and at 50 ms a three-slot spread costs 150 ms per hop,
            # which dominates the latency budget entirely.
            rank = ((255 - min(quality, 255)) // 96) if self.STAGGER else 0

            # If the chosen slot already holds another payload of equal priority the
            # scheduler refuses, and a refused relay is a silently dropped one — the
            # frame simply stops there and everyone downstream loses it. Measured at ~80%
            # loss on a hop whose relay gate was wide open. Try the next slots instead;
            # arriving a slot late is worth far more than not arriving.
            # Do not transmit in a phase this radio needs its ears for. A relay goes out
            # in the slot right after reception, so a radio carrying one conversation
            # transmits in exactly the phase a second conversation arrives on — and goes
            # deaf to it. Measured at a mid-chain radio: one stream heard 227 times and
            # relayed every time, the other heard 27 times out of 243.
            #
            # Four slots leaves room for two conversations: two phases listening, two
            # transmitting.
            spf = self.cfg.slots_per_frame
            fresh = self.cfg.slots_per_frame * 2
            recent = {ph for ph, seen in rx.rx_phases.items() if (slot - seen) < fresh}
            # Protect ears only when a second CONVERSATION is present. Hearing several
            # neighbours on different phases is ordinary — they are relaying the same
            # talker — and blocking the next slot for that costs the pipelining rule far
            # more than the deafness it avoids.
            sources = {rx.rx_sources.get(ph) for ph in recent}
            busy = (recent - {slot % spf}) if len(sources) > 1 else set()
            for attempt in range(rank, rank + spf * 2):
                if ((slot + attempt + 1) % spf) in busy:
                    continue
                if rx.sched.relay(pdu, slot + attempt, rx.addr) == 0:
                    rx.relayed += 1
                    break

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
        """Fraction of that source's payloads each node actually received."""
        origin = next((n for n in self.nodes if n.addr == src_addr), None)
        sent = origin.originated if origin else 0
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
