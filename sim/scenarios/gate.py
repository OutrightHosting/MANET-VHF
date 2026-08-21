"""
The Phase 0 gate.

The brief sets five questions and three success criteria. This runs them and reports
pass or fail against each, so the gate is a command rather than a judgement call.

  Questions                                        Criteria
  1. Does MPR converge with 12 nodes moving?       converges with 12 mobile nodes
  2. Does the cluster case produce zero relaying?  cluster case relays nothing
  3. Does pipelining resolve across 3-5 hops?      5-hop chain within 300 ms
  4. Beacon overhead as % of channel capacity?
  5. What happens on partition and rejoin?
"""

from ..manet.core import CONFIG
from ..manet.mobility import DispersingGroup, SplinterRejoin
from ..manet.radio import ENVIRONMENTS, LinkBudget, usable_range_m
from ..manet.world import Simulation

WOOD = ENVIRONMENTS["woodland"]
BUDGET = LinkBudget()


def _secs(slots):
    return slots * CONFIG.slot_us / 1e6


def cluster(n=12, seconds=90):
    """Twelve leaders standing together. Nothing should relay."""
    from ..manet.mobility import DispersingGroup as DG
    mob = DG(n, spread_min=120.0, spread_max=120.0, period_s=1e9)
    sim = Simulation(mob, WOOD, BUDGET, talker=0)
    settle = CONFIG.beacon_interval_slots * 4
    sim.run(settle)
    total = int(seconds / (CONFIG.slot_us / 1e6))
    sim.run(total, voice_from=settle)

    d = sim.delivery(sim.nodes[0].addr)
    return {
        "relays_selected": sim.relaying_now(),
        "relay_transmissions": sim.relay_total(),
        "min_delivery": min(d.values()) if d else 0.0,
        "reachable": sim.reachable_from(0),
        "nodes": n,
    }


def dispersal(n=12, cycles=2, samples=60, talk_s=8.0, gap_s=20.0):
    """
    The group stretches out and gathers up again while someone is talking.

    Voice is push-to-talk and bursty — someone transmits for a few seconds, then the
    channel is idle. Continuous transmission is not an operational case, and testing
    against it starves the control plane in a way real use never would.
    """
    mob = DispersingGroup(n, spread_min=120.0, spread_max=3000.0, period_s=240.0)
    sim = Simulation(mob, WOOD, BUDGET, talker=0)
    settle = CONFIG.beacon_interval_slots * 4
    sim.run(settle)

    total = int(cycles * 240.0 / (CONFIG.slot_us / 1e6))
    step = max(total // samples, 1)
    talk = int(talk_s / (CONFIG.slot_us / 1e6))
    cycle = talk + int(gap_s / (CONFIG.slot_us / 1e6))
    trace, converged_samples = [], 0
    done = 0
    while done < total:
        chunk = min(step, total - done)
        # PTT: transmitting for talk_s out of every cycle, idle in between.
        base = settle + ((sim.slot - settle) // cycle) * cycle
        sim.run(chunk, voice_from=base, voice_to=base + talk)
        done += chunk
        t = _secs(sim.slot) - _secs(settle)
        spread = mob.spread_at(t)
        ok = sim.converged()
        converged_samples += 1 if ok else 0
        trace.append((t, spread, sim.relaying_now(), sim.reachable_from(0), ok))

    d = sim.delivery(sim.nodes[0].addr)
    return {
        "samples": len(trace),
        "converged_fraction": converged_samples / max(len(trace), 1),
        "trace": trace,
        "min_delivery": min(d.values()) if d else 0.0,
        "mean_delivery": sum(d.values()) / len(d) if d else 0.0,
        "relay_transmissions": sim.relay_total(),
    }


def partition(n=12, samples=60):
    """The group splits, goes out of contact, and comes back."""
    mob = SplinterRejoin(n, separation_m=4000.0, out_s=90.0, away_s=120.0, back_s=90.0)
    sim = Simulation(mob, WOOD, BUDGET, talker=0)
    settle = CONFIG.beacon_interval_slots * 4
    sim.run(settle)

    total = int(400.0 / (CONFIG.slot_us / 1e6))
    step = max(total // samples, 1)
    trace = []
    done = 0
    while done < total:
        chunk = min(step, total - done)
        sim.run(chunk, voice_from=settle, voice_to=settle + total)
        done += chunk
        t = _secs(sim.slot) - _secs(settle)
        trace.append((t, mob.phase_at(t), sim.reachable_from(0),
                      sim.reachable_from(n - 1), sim.converged()))

    # how long after the halves are back in contact before everything reconverges
    rejoin_t = 90.0 + 120.0 + 90.0
    # Healed = topology converged and everyone reachable. Reported alongside the actual
    # reachability, because a partition that "heals" to 11 of 12 has not healed — and
    # that is exactly what happens when beacon starvation leaves one radio without a
    # confirmed two-way link. See OQ-0009.
    heal, heal_partial, final_reach = None, None, 0
    for t, phase, reach_a, _reach_b, ok in trace:
        if t < rejoin_t:
            continue
        final_reach = reach_a
        if ok and reach_a >= n - 1 and heal_partial is None:
            heal_partial = t - rejoin_t
        if ok and reach_a == n and heal is None:
            heal = t - rejoin_t
    return {"trace": trace, "heal_s": heal, "heal_partial_s": heal_partial,
            "final_reach": final_reach, "rejoin_at_s": rejoin_t, "nodes": n}


def latency(hops=6):
    """Slots, and milliseconds, from the talker's mouth to each relay down the chain."""
    reach = usable_range_m(WOOD, BUDGET)
    pos = [(i * 0.9 * reach, 0.0) for i in range(hops + 1)]
    sim = Simulation(pos, WOOD, BUDGET, talker=0)
    settle = CONFIG.beacon_interval_slots * 4
    sim.run(settle)
    sim.run(1200, voice_from=settle)
    lat = sim.latency_slots(sim.nodes[0].addr)
    return {i: (v, v * CONFIG.slot_us / 1000.0) for i, v in sorted(lat.items())}


def multi_talker(hops=7, talkers=(0, 3), slots=1500):
    """
    Several people talking at once.

    Absent from the brief's criteria and therefore untested for the whole of Phase 0,
    which is how a total failure went unnoticed: every scenario had exactly one talker,
    and with one talker the design looked correct.
    """
    from ..manet.core import VOICE, Pdu

    class Multi(Simulation):
        def _schedule_voice(self, slot, active):
            if not active:
                return
            for ti in talkers:
                n = self.nodes[ti]
                if slot % self.cfg.slots_per_frame != self.voice_phase(n):
                    continue
                pdu = Pdu(src=n.addr, prev=n.addr, dst=0xC0, type=VOICE, seq=n.seq & 0xFF)
                n.seq = (n.seq + 1) & 0xFF
                if n.sched.originate(pdu, slot) == 0:
                    n.originated += 1
                    self.origin_log.setdefault((pdu.src, pdu.seq), []).append(slot)
                    n.heard_payloads.setdefault(
                        self._payload_id(pdu.src, pdu.seq, slot), slot)

    reach = usable_range_m(WOOD, BUDGET)
    pos = [(i * 0.9 * reach, 0.0) for i in range(hops)]
    sim = Multi(pos, WOOD, BUDGET, talker=talkers[0])
    settle = CONFIG.beacon_interval_slots * 4
    sim.run(settle)
    sim.run(slots - settle, voice_from=settle)

    out = {}
    for ti in talkers:
        d = sim.delivery(sim.nodes[ti].addr)
        # does this stream reach the far side of every other talker?
        others = [t for t in talkers if t != ti]
        out[ti] = {"profile": [d.get(i, 0.0) for i in range(hops)],
                   "crosses": min(d.get(t, 0.0) for t in others)}
    return out
