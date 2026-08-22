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
from ..manet.geometry import hop_span_m, horizon_m, severed_m
from ..manet.radio import ENVIRONMENTS, LinkBudget, usable_range_quantiles
from ..manet.world import Simulation

WOOD = ENVIRONMENTS["woodland"]
BUDGET = LinkBudget()

# EVERY GEOMETRIC PARAMETER BELOW IS DERIVED FROM THIS, NOT HARDCODED.
#
# Both the mobility and partition criteria were previously written against fixed
# distances -- 3000 m of stretch and 4000 m of separation. They were chosen when woodland
# single-hop range was 528 m. Correcting the vegetation model (OQ-0023) took that to
# 4416 m and the constants did not move, so for some time:
#
#   Q1 stretched the group to 3000 m inside a 4416 m radio horizon -> never left one hop
#   Q5 separated the halves by 4000 m, and with +/-150 m of cluster jitter the closest
#      cross-boundary pair sat at 3700 m -> never actually partitioned
#
# Neither criterion could fail, and both reported PASS for weeks. Deriving them from the
# measured range means the next propagation change moves the test with it.
RANGE_M = horizon_m(WOOD, BUDGET)
# p90 is the SHORTEST: the distance nine links in ten will manage. M-03.
RANGE_P90, _, RANGE_P10 = usable_range_quantiles(WOOD, BUDGET)

# Stretch far enough that the group is genuinely several hops deep at full extension.
DISPERSAL_HOPS = 6
# Scatter within each half. A physical fact about how a group stands, not a radio
# distance, so it is named rather than derived. cluster_m in SplinterRejoin.
CLUSTER_JITTER_M = 150.0

# Dense-cover case (OQ-0032). A target horizon and the spreads to sweep it over.
DENSE_HORIZON_M = 350.0        # geometry-exempt: target horizon, not a spacing
DENSE_SPREADS_M = (500.0, 1000.0, 1500.0, 2000.0, 3000.0)   # geometry-exempt: sweep axis

OUT_S, AWAY_S, BACK_S = 90.0, 120.0, 90.0  # geometry-exempt: seconds, not metres


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


def dispersal(n=12, cycles=2, samples=60, talk_s=8.0, gap_s=20.0, spread_max=None):
    """
    The group stretches out and gathers up again while someone is talking.

    Voice is push-to-talk and bursty — someone transmits for a few seconds, then the
    channel is idle. Continuous transmission is not an operational case, and testing
    against it starves the control plane in a way real use never would.
    """
    # Spread is bounded above by what the TTL can carry -- beyond that the test measures
    # MANET_VOICE_TTL rather than the protocol -- and bounded BELOW by the radio horizon,
    # which is the bound that was missing. DISPERSAL_HOPS * RANGE_M satisfies both.
    if spread_max is None:
        spread_max = hop_span_m(WOOD, BUDGET, DISPERSAL_HOPS)
    mob = DispersingGroup(n, spread_min=120.0, spread_max=spread_max, period_s=240.0)
    sim = Simulation(mob, WOOD, BUDGET, talker=0)
    settle = CONFIG.beacon_interval_slots * 4
    sim.run(settle)

    total = int(cycles * 240.0 / (CONFIG.slot_us / 1e6))
    step = max(total // samples, 1)
    talk = int(talk_s / (CONFIG.slot_us / 1e6))
    cycle = talk + int(gap_s / (CONFIG.slot_us / 1e6))
    trace, converged_samples, max_depth = [], 0, 0
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
        depth = sim.depth_from(0)
        max_depth = max(max_depth, depth)
        trace.append((t, spread, sim.relaying_now(), sim.reachable_from(0), ok, depth))

    d = sim.delivery(sim.nodes[0].addr)
    return {
        "samples": len(trace),
        "converged_fraction": converged_samples / max(len(trace), 1),
        "trace": trace,
        "min_delivery": min(d.values()) if d else 0.0,
        "mean_delivery": sum(d.values()) / len(d) if d else 0.0,
        "relay_transmissions": sim.relay_total(),
        # B-04c: delivery divides by payloads that reached the air, so quote the PTT
        # success beside it. Equal to 1.0 means the two are the same number.
        "ptt_success": sim.ptt_success(sim.nodes[0].addr),
        # PRECONDITION. A mobility test that never leaves one hop tests nothing, and this
        # one did not for weeks. The runner fails the criterion if this is under 2.
        "max_hop_depth": max_depth,
        "spread_max": spread_max,
        "range_m": RANGE_M,
        # Range is a distribution, not a number. M-03.
        "range_p90": RANGE_P90,
        "range_p10": RANGE_P10,
    }


def partition(n=12, samples=60, separation_m=None):
    """The group splits, goes out of contact, and comes back."""
    # Must exceed one radio horizon plus the cluster jitter at both ends, or the halves
    # stay in contact and the test measures nothing. It did not, and they did.
    if separation_m is None:
        # Jitter passed explicitly: Q5's old 4000 m failed partly because +/-150 m of
        # cluster scatter put the closest cross-boundary pair at 3700 m and nobody
        # had subtracted it.
        separation_m = severed_m(WOOD, BUDGET, jitter_m=CLUSTER_JITTER_M)
    mob = SplinterRejoin(n, separation_m=separation_m, cluster_m=CLUSTER_JITTER_M,
                         out_s=OUT_S, away_s=AWAY_S, back_s=BACK_S)
    sim = Simulation(mob, WOOD, BUDGET, talker=0)
    settle = CONFIG.beacon_interval_slots * 4
    sim.run(settle)

    total = int(400.0 / (CONFIG.slot_us / 1e6))
    step = max(total // samples, 1)
    # M-05: also track whether the FAR HALF is receiving voice, not only whether the
    # tables have caught up. Barrage floods, so it does not need converged tables to
    # deliver -- reporting table convergence alone made recovery look far worse than the
    # service actually was, which is how B-06 arrived as "7.6x slower".
    far = list(range(n // 2, n))
    trace = []
    done = 0
    while done < total:
        chunk = min(step, total - done)
        before = {i: len(sim.nodes[i].heard_payloads) for i in far}
        sim.run(chunk, voice_from=settle, voice_to=settle + total)
        done += chunk
        t = _secs(sim.slot) - _secs(settle)
        hearing = sum(1 for i in far if len(sim.nodes[i].heard_payloads) > before[i])
        trace.append((t, mob.phase_at(t), sim.reachable_from(0),
                      sim.reachable_from(n - 1), sim.converged(), hearing, len(far)))

    # how long after the halves are back in contact before everything reconverges
    rejoin_t = OUT_S + AWAY_S + BACK_S
    # Healed = topology converged and everyone reachable. Reported alongside the actual
    # reachability, because a partition that "heals" to 11 of 12 has not healed — and
    # that is exactly what happens when beacon starvation leaves one radio without a
    # confirmed two-way link. See OQ-0009.
    # PRECONDITION: were the halves ever actually out of contact? If the front could still
    # see all twelve while "apart", nothing was partitioned and the heal time is fiction.
    apart_reach = [ra for t, phase, ra, _rb, _ok, _h, _n in trace if phase == "apart"]
    truly_split = bool(apart_reach) and max(apart_reach) <= n // 2

    # Time until the far half is hearing voice again -- the number that means "the
    # network is working", as against "the tables agree".
    voice_back = None
    for t, phase, _ra, _rb, _ok, hearing, nfar in trace:
        if t >= rejoin_t and hearing >= nfar and voice_back is None:
            voice_back = t - rejoin_t

    heal, heal_partial, final_reach = None, None, 0
    for t, phase, reach_a, _reach_b, ok, _hearing, _nfar in trace:
        if t < rejoin_t:
            continue
        final_reach = reach_a
        if ok and reach_a >= n - 1 and heal_partial is None:
            heal_partial = t - rejoin_t
        if ok and reach_a == n and heal is None:
            heal = t - rejoin_t
    return {"trace": trace, "heal_s": heal, "heal_partial_s": heal_partial,
            "final_reach": final_reach, "rejoin_at_s": rejoin_t, "nodes": n,
            "truly_split": truly_split, "voice_back_s": voice_back,
            "worst_reach_while_apart": max(apart_reach) if apart_reach else n,
            "separation_m": separation_m, "range_m": RANGE_M}


def latency(hops=4):
    """Slots, and milliseconds, from the talker's mouth to each relay down the chain."""
    reach = horizon_m(WOOD, BUDGET)
    pos = [(i * 0.9 * reach, 0.0) for i in range(hops + 1)]  # within the TTL
    sim = Simulation(pos, WOOD, BUDGET, talker=0)
    settle = CONFIG.beacon_interval_slots * 4
    sim.run(settle)
    sim.run(1200, voice_from=settle)
    lat = sim.latency_slots(sim.nodes[0].addr)
    return {i: (v, v * CONFIG.slot_us / 1000.0) for i, v in sorted(lat.items())}


def dense_cover(n=12, slots=3000):
    """
    Short hops. The one geometry none of the brief's criteria contains.

    Every other scenario here is the repeater triangle -- long hops, terrain doing the
    blocking. In thick woodland the links shorten, the same ground costs more hops, and
    MANET_VOICE_TTL runs out before the group does. The radios stay connected to each
    other the whole time; voice simply cannot cross the group.

    Reports the spread at which the group splits, because "keep everyone inside N metres in
    thick cover" is an instruction somebody can follow, and discovering it in a forest is
    not. OQ-0032.
    """
    from ..scenarios.dense_cover import env_for_horizon, line_of_leaders

    env = env_for_horizon(DENSE_HORIZON_M)
    out = []
    for spread in DENSE_SPREADS_M:
        sim, d = line_of_leaders(n, spread, env, slots=slots)
        heard = sum(1 for v in d.values() if v > 0.5)
        out.append({"spread_m": spread, "reachable": sim.reachable_from(0),
                    "hearing": heard, "nodes": n, "worst": min(d.values())})
    intact = [r for r in out if r["hearing"] == n]
    return {"horizon_m": DENSE_HORIZON_M,
            "rows": out,
            "safe_spread_m": max((r["spread_m"] for r in intact), default=0.0),
            "ttl": CONFIG.voice_ttl}


def multi_talker(hops=7, talkers=(0, 3), slots=6000):
    """
    Several people talking at once.

    Absent from the brief's criteria and therefore untested for the whole of Phase 0,
    which is how a total failure went unnoticed: every scenario had exactly one talker,
    and with one talker the design looked correct.

    This used to carry its OWN copy of _schedule_voice, and nothing called it. The copy
    never gained the dedup registration that stops a talker relaying its own echo, never
    gained a TTL on the PDU, and never gained PTT accounting -- so even when run it would
    have measured a protocol nobody ships. An unexercised second code path is worse than
    none: it looks like coverage. Simulation now takes `talkers` and there is one path.

    Reports speech_through, not delivery. delivery() divides by payloads that reached the
    air, so a talker denied the channel has its refused payloads vanish from the
    denominator and its score inflated -- which is exactly the failure this scenario
    exists to catch (B-04b, B-04c).
    """
    reach = horizon_m(WOOD, BUDGET)
    pos = [(i * 0.9 * reach, 0.0) for i in range(hops)]
    sim = Simulation(pos, WOOD, BUDGET, talker=talkers[0], talkers=talkers)
    settle = CONFIG.beacon_interval_slots * 4
    sim.run(settle)
    sim.run(slots - settle, voice_from=settle)

    streams = {}
    for ti in talkers:
        a = sim.nodes[ti].addr
        through = sim.speech_through(a)
        streams[ti] = {
            "ptt": sim.ptt_success(a),
            "per_node": [through.get(i, 0.0) for i in range(hops)],
            "mean": sum(through.values()) / len(through) if through else 0.0,
        }
    return {"hops": hops, "talkers": list(talkers), "streams": streams,
            "worst_stream": min(v["mean"] for v in streams.values()),
            "worst_ptt": min(v["ptt"] for v in streams.values())}
