"""
The simulation loop.

Python owns the clock, the radio channel and the scenario. Every protocol decision —
what to transmit, when, whether to relay, who relays for whom — is made by the C core
through sim/manet/core.py. Nothing in this file implements protocol behaviour, and if it
starts to, that is a bug rather than a shortcut (ADR-0006).
"""

import collections

from .core import (
    BEACON, VOICE, PRIO_SIGNALLING, PRIO_VOICE, BROADCAST, CONFIG,
    Dedup, MprSet, NeighbourTable, Pdu, Scheduler,
)
from .core import is_control_slot, voice_phase

# One control slot per superframe. Derived here because the C config does not export it.
SUPERFRAME_SLOTS = 32
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
        self.last_origination_attempt = None
        # PTT success. delivery() divides by payloads that reached the air, so a talker
        # locked out of the channel has its refused payloads vanish from the denominator
        # and its delivery percentage INFLATED. This counts the attempts.
        self.origination_attempts = 0
        self.rx_phases = {}
        # Which ORIGIN each receive phase carried. Hearing several neighbours is normal
        # and not a reason to protect ears; hearing two different conversations is.
        self.rx_sources = {}

        # Channel access state. A radio that can hear a stream in progress holds its
        # beacon rather than keying up over it.
        self.last_voice_slot = None
        self.beacon_deferrals = 0
        self.last_beacon = None
        # Set when this radio's symmetric neighbour set changes, so the next NAMA win
        # beacons regardless of the interval. See _schedule_beacons.
        self.topology_changed = False
        self.known_symmetric = frozenset()

        # metrics
        self.heard_payloads = {}   # (src, seq) -> slot first decoded
        self.relayed = 0
        self.originated = 0
        self.beacons_sent = 0
        self.decode_failures = 0


class Simulation:
    def __init__(self, positions, env, budget=None, talker=0, terrain=None,
                 talkers=None):
        self.cfg = CONFIG
        self.channel = Channel(env, budget, terrain)
        # `positions` may be a fixed list or a Mobility. Nodes carry a current position
        # that is refreshed once per frame — at walking pace a frame is 7 cm, so there is
        # nothing to gain from updating per slot.
        self.mobility = positions if hasattr(positions, "positions_at") else Static(positions)
        start = self.mobility.positions_at(0)
        self.nodes = [Node(i, i + 1, p) for i, p in enumerate(start)]
        # ONE voice path, however many people are talking.
        #
        # Multiple talkers used to mean a separate _schedule_voice in gate.py, and that
        # duplicate is precisely why B-04b went unnoticed: it never gained the dedup
        # registration that stops a talker relaying its own echo, never gained a TTL on the
        # PDU, never gained PTT accounting, and nothing ever called it anyway. A second
        # code path that is not exercised is worse than no path at all -- it looks like
        # coverage.
        self.talker = talker
        self.talkers = list(talkers) if talkers else [talker]
        self.slot = 0
        self.tx_log = []
        # Where the per-hop slot cost goes. ADR-0002 promises one hop per slot; the gate
        # chain measures 1.7. These counters say which of the three gates spends it.
        self.why = collections.Counter()
        self.delay_hist = collections.Counter()
        self.contender_hist = collections.Counter()
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
    NAMA_RELAY = True
    PROTECT_TALKER_PHASE = False
    BARRAGE_RELAY = True
    # Slots after an origination attempt during which a radio still guards its own voice
    # phase. One frame is too short — PTT leaves gaps between payloads — and permanent is
    # too long, since every radio would then protect a phase it never uses.
    TALKER_MEMORY = 16
    # Keep voice out of the reserved signalling slot. ON -- this is the B-15 fix.
    # It was off because excluding voice from 3.1% of slots took the 7-hop chain from 84.0%
    # to 74.5%. That nine-point cost was the originator LOSING the payload that landed in
    # the slot, not the reservation itself; place() in the core now steps voice over the
    # slot instead, and origination count is unchanged at every density. What the
    # reservation buys is the thing B-15 turned out to be: somewhere for beacons to go that
    # is not on top of live voice.
    CONTROL_SLOTS = True
    DESIGNATED_RELAY = False

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
            if (n.last_beacon is not None and (slot - n.last_beacon) < interval
                    and not n.topology_changed):
                continue
            # Win this slot?
            if not n.nb.nama_wins(slot):
                continue
            # Beacons go in the reserved signalling slot, and only there. This reverses
            # an earlier decision, and the reason it was right then and wrong now is the
            # supply: at one control slot per superframe there were far fewer slots than
            # radios, so waiting for one starved beaconing outright and pushed recovery
            # from 40 s to 112 s. At MANET_SIGNAL_SLOT_PERIOD there is one every eight
            # slots -- four times the supply the old superframe scheme offered, while
            # taking none of them on top of voice.
            #
            # This is the other half of B-15. Reserving the slot is what gives beacons
            # somewhere to go; confining beacons to it is what stops them going anywhere
            # else. Without this half the reservation is airtime given up for nothing --
            # beacons would still land on live voice in the other three slots.
            if self.CONTROL_SLOTS and not is_control_slot(slot):
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
                n.topology_changed = False

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
        return self.phase_of_addr(node.addr)

    def phase_of_addr(self, addr):
        """
        The voice phase belonging to an address.

        Now answered by core/src/slot.c rather than reimplemented here. It was harness-only
        until B-04, which by ADR-0006's rule meant the most fundamental MAC decision in the
        system -- when a talker transmits -- was not built.
        """
        return voice_phase(addr)

    def _schedule_voice(self, slot, active):
        if not active:
            return
        for ti in self.talkers:
            self._originate_voice(self.nodes[ti], slot)

    def _originate_voice(self, n, slot):
        # One Codec2 payload per frame, beginning at this radio's own phase.
        if slot % self.cfg.slots_per_frame != self.voice_phase(n):
            return
        # Control slots belong to the topology, not to voice -- but a talker whose phase
        # lands on one steps over it rather than losing the payload. manet_sched_originate
        # goes through place(), which does the stepping in the core. Dropping here instead
        # is what made the reservation look unaffordable, and with the period a multiple of
        # the frame it would silently cost one voice phase in four every one of its
        # payloads while the other three lost none.
        # Voice TTL is bounded by the mouth-to-ear budget, not by the field width.
        pdu = Pdu(src=n.addr, prev=n.addr, dst=0xC0, type=VOICE,
                  seq=n.seq & 0xFF, prio=PRIO_VOICE, ttl=self.cfg.voice_ttl)
        n.seq = (n.seq + 1) & 0xFF
        n.last_origination_attempt = slot
        n.origination_attempts += 1
        if n.sched.originate(pdu, slot) == 0:
            # Register our own payload in our own dedup table. Without this the echo
            # coming back off the relays looks like a brand new frame, and the talker
            # relays its own voice — measured at 273 spurious transmissions against 564
            # originations, and because those land in every phase but the talker's own,
            # they were what smeared the pipelining discipline ADR-0002 depends on.
            n.dedup.check(pdu.src, pdu.seq, slot)
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
                if any(self.channel.rx_dbm_between(rx.pos, positions[i]) >=
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
        # TRIGGERED UPDATE. Waiting out the full beacon interval after the topology moves
        # is what makes reconvergence slow, and barrage makes it slower still -- not by
        # sending fewer beacons (measured: 8% fewer) but by putting 42% more transmissions
        # in the air, so more receivers are deaf or collided when one goes out. A radio
        # that has just gained or lost a symmetric neighbour has something worth saying
        # and should say it at the next slot it wins, not in up to 5.28 s.
        now_sym = frozenset(rx.nb.symmetric())
        if now_sym != rx.known_symmetric:
            rx.known_symmetric = now_sym
            rx.topology_changed = True

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
            # Never transmit in the talker's own phase. With four slots the pipeline
            # wraps after four hops into the slot the originator is using for its NEXT
            # payload, so the deepest relays deafen the whole neighbourhood to the
            # source. The phase is a hash of the address and `src` is in every header,
            # so every relay can derive it from the frame in front of it — no signalling.
            if self.PROTECT_TALKER_PHASE:
                busy = busy | {self.phase_of_addr(pdu.src)}
            # A DESIGNATED relay does not wait. If the sender has already named this
            # radio as one of its multipoint relays — which it advertises in every beacon,
            # and beacons now get through because NAMA gives them collision-free slots —
            # then there is nothing to discover and nothing to elect. Relay in the very
            # next slot, which is the pipelining rule as originally intended.
            #
            # This was abandoned when beacons could not get through: a radio that missed
            # the beacon naming it simply did not relay, and one closed gate blacked out
            # the chain. Receiver-decided relaying fixed that at the cost of an election
            # wait of 41-81 ms per hop, which is half the per-hop budget. With beacons
            # reliable, the wait is no longer buying anything on the primary path.
            #
            # The receiver-decided path stays as the BACKUP: if no designated relay
            # carries the frame, whoever reaches somewhere new still picks it up a slot
            # or two later. Primary is fast, backup is thorough.
            # OFF by default. It removes the election wait and buys a fourth hop, but a
            # single designated relay has no backup: measured 75.7% -> 56.2% through the
            # hill, which is the case the product exists for. Williams & Camp found the
            # same thing — sender-decided selection degrades where receiver-decided
            # self-heals. Four fragile hops is not better than three robust ones, and
            # neither meets the requirement; the preamble does.
            if self.DESIGNATED_RELAY and rx.nb.should_relay_for(pdu.prev):
                if rx.sched.relay(pdu, slot, rx.addr) == 0:
                    rx.relayed += 1
                return

            # Who relays, when several could. Four radios on a hilltop all hear the valley
            # below, all correctly conclude they reach somewhere the sender does not, and
            # all fire in the same slot — measured at 1.5% delivery through the hill.
            # Ranking by link quality separates three of them and reached 49%. NAMA
            # separates all of them, because it already guarantees exactly one winner
            # within two hops, which is exactly the contention here.
            # The radios actually in contention are those that also heard this frame —
            # our neighbours that are also the sender's neighbours — not everyone within
            # two hops.
            mine = set(rx.nb.symmetric())
            contenders = [a for a in mine if rx.nb.reaches_addr(pdu.prev, a)] or list(mine)

            # BARRAGE. If identical concurrent copies combine rather than collide
            # (Channel.CONCURRENT_IDENTICAL), there is nothing to elect: everyone holding
            # the frame fires in the very next slot, which is ADR-0002's pipelining rule
            # exactly as written and exactly as TrellisWare TSM and Glossy implement it.
            # The election existed only to stop simultaneous relays jamming each other.
            if self.BARRAGE_RELAY:
                # NEVER RELAY IN YOUR OWN ORIGINATION PHASE.
                #
                # Without this a radio adjacent to another talker's flood relays that
                # stream in exactly the slot it needs for its own voice, every frame, and
                # the scheduler then refuses its origination. It does not degrade the
                # radio, it MUTES it: measured at 48 nodes, one talker keyed up 0 times in
                # 500 attempts over 300 s while the other delivered 100%.
                #
                # The election never had this failure because its slot search consulted
                # `busy` (the guard immediately above) and its jitter acted as accidental
                # protection. The barrage branch returned before reaching either.
                #
                # Abstaining is nearly free here and that is the whole point of barrage:
                # 3.7-9.4 radios carry each payload concurrently, so one standing down
                # costs a copy. Being muted costs the call.
                # ...but only while this radio is actually a talker. A radio with nothing
                # to say has no origination to protect, and yielding for it is expensive:
                # on a sparse chain the single forward relay abstains and the frame dies
                # there. Measured — the unconditional form failed the mobility criterion.
                # Never relay INTO a control slot -- but step over it, do not abandon the
                # frame. Under barrage the flood fires in every slot unconditionally, which
                # is why beacon reception fell from 26.9% to 21.8%: NAMA keeps beacons clear
                # of each other and nothing kept voice clear of them. place() in the core
                # advances a voice frame past the reserved slot, so the pipeline stretches
                # by one slot instead of breaking. Returning here was the hole that made a
                # reservation read as nine points of loss.
                talking = (rx.last_origination_attempt is not None
                           and (slot - rx.last_origination_attempt) < self.TALKER_MEMORY)
                if talking and (slot + 1) % spf == self.phase_of_addr(rx.addr):
                    self.why["yielded own voice phase"] += 1
                    return
                if rx.sched.relay(pdu, slot, rx.addr) == 0:
                    rx.relayed += 1
                    self.why["relayed"] += 1
                    self.delay_hist[0] += 1
                else:
                    self.why["scheduler refused"] += 1
                return

            for attempt in range(rank, rank + spf * 4):
                target = slot + attempt
                if ((target + 1) % spf) in busy:
                    self.why["busy"] += 1
                    continue
                if self.NAMA_RELAY and not rx.nb.nama_wins_among(target + 1, contenders):
                    self.why["lost election"] += 1
                    continue
                if rx.sched.relay(pdu, target, rx.addr) == 0:
                    rx.relayed += 1
                    self.why["relayed"] += 1
                    self.delay_hist[attempt] += 1
                    self.contender_hist[len(contenders)] += 1
                    break
                self.why["scheduler refused"] += 1
            else:
                self.why["gave up"] += 1

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

    def depth_from(self, index):
        """
        Hop depth of the furthest node from `index` over symmetric links.

        Exists because `reachable_from` counts nodes and says nothing about how far
        apart they are — a fully-connected cluster and a five-hop chain both report 12.
        The gate needs the difference, and did not have it.
        """
        seen = {self.nodes[index].addr}
        frontier = [self.nodes[index]]
        by_addr = {n.addr: n for n in self.nodes}
        depth = 0
        while frontier:
            nxt = []
            for n in frontier:
                for a in n.nb.symmetric():
                    if a not in seen and a in by_addr:
                        seen.add(a)
                        nxt.append(by_addr[a])
            if nxt:
                depth += 1
            frontier = nxt
        return depth

    def relay_total(self):
        return sum(n.relayed for n in self.nodes)

    def delivery(self, src_addr):
        """
        Fraction of that source's AIRED payloads each node received.

        Note the denominator, because it has a bias in it and the bias is not small.
        `originated` counts payloads the scheduler ACCEPTED. A radio locked out of the
        channel has its refused payloads vanish from the denominator, so it can read 64%
        delivered while emitting almost nothing -- measured, during B-04b.

        Single-talker figures are unaffected: PTT success is 100% in the chain, hill and
        cluster cases, so aired == attempted and this equals end-to-end. With concurrent
        talkers it does not, and `speech_through()` is the number that matters.
        """
        origin = next((n for n in self.nodes if n.addr == src_addr), None)
        sent = origin.originated if origin else 0
        if sent == 0:
            return {}
        out = {}
        for n in self.nodes:
            got = sum(1 for pid in n.heard_payloads if pid[0] == src_addr)
            out[n.index] = got / sent
        return out

    def ptt_success(self, src_addr):
        """
        Fraction of this radio's origination attempts that reached the air.

        Below 1.0 the radio is being denied the channel -- someone pressed the button and
        nothing went out. It is the half of the story `delivery()` cannot show.
        """
        origin = next((n for n in self.nodes if n.addr == src_addr), None)
        if origin is None or origin.origination_attempts == 0:
            return 1.0
        return origin.originated / origin.origination_attempts

    def speech_through(self, src_addr):
        """
        End to end: of the speech this radio TRIED to send, what fraction arrived, per node.

        delivery x ptt_success. The only figure that means "could the other person hear
        them", and the only one safe to quote when more than one radio is talking.
        """
        ptt = self.ptt_success(src_addr)
        return {i: d * ptt for i, d in self.delivery(src_addr).items()}

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
