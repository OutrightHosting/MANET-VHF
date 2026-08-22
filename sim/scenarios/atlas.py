"""
The whole range of shapes a group can be in, measured the same way.

Every other scenario file here answers one question. This one exists so the shapes can be
compared side by side: a pair, a cluster, a line, clusters joined by singles, the repeater
triangle, a mountain, thick woodland, and the same geometries at scale.

Emits JSON so it can be drawn.

  python3 -m sim.scenarios.atlas
"""
import json
import random

from ..manet.core import CONFIG
from ..manet.geometry import horizon_m, within_one_hop_m
from ..manet.mobility import Static
from ..manet.radio import ENVIRONMENTS, Environment, LinkBudget, usable_range_m
from ..manet.terrain import Flat, Ridge
from ..manet.world import Simulation

BUD = LinkBudget()
# How far apart people stand when they are "together". A fact about human beings, not a
# radio distance, so it is named rather than derived from the horizon.
SHOULDER = 120.0        # geometry-exempt: spacing between people, not a link distance
ARMS_LENGTH = 70.0      # geometry-exempt: spacing between people, not a link distance
NEAR = 62.5             # geometry-exempt: spacing between people, not a link distance
STAGGER = 40.0          # geometry-exempt: spacing between people, not a link distance
ROW = 50.0              # geometry-exempt: spacing between people, not a link distance
PAIR = 60.0             # geometry-exempt: spacing between people, not a link distance
# The thick-woodland case: a target horizon and two group spreads to compare. OQ-0032.
THICK_HORIZON = 350.0   # geometry-exempt: target horizon, not a spacing
SPREAD_SPLIT = 2000.0   # geometry-exempt: sweep value for OQ-0032
SPREAD_OK = 1000.0      # geometry-exempt: sweep value for OQ-0032
WOOD = ENVIRONMENTS["woodland"]
OPEN = ENVIRONMENTS["open"]


def dense(target_m):
    """An environment whose single-hop range is `target_m`."""
    lo, hi = 2.0, 9.0
    for _ in range(60):
        mid = (lo + hi) / 2.0
        if usable_range_m(Environment("x", exponent=mid, foliage=True), BUD) > target_m:
            lo = mid
        else:
            hi = mid
    return Environment(f"dense ({target_m:.0f} m hops)", exponent=lo, foliage=True)


def scatter(n, extent_m, seed, min_gap=None):
    """
    Radios spread over an AREA, not a line.

    A real group does not form a queue. It spreads across ground with several neighbours
    each and more than one route between any two people, which is what every textbook
    MANET diagram shows and what none of the line scenarios here reproduce.
    """
    rng = random.Random(seed)
    gap = min_gap if min_gap is not None else extent_m / (n ** 0.5) * 0.45
    pts = []
    for _ in range(n * 400):
        if len(pts) == n:
            break
        c = (rng.uniform(0.0, extent_m), rng.uniform(0.0, extent_m * 0.62))
        if all((c[0]-q[0])**2 + (c[1]-q[1])**2 > gap*gap for q in pts):
            pts.append(c)
    while len(pts) < n:                     # relax rather than loop forever
        pts.append((rng.uniform(0.0, extent_m), rng.uniform(0.0, extent_m * 0.62)))
    return pts


def centre_of(pos):
    """Index of the radio nearest the middle of the group."""
    cx = sum(p[0] for p in pos) / len(pos)
    cy = sum(p[1] for p in pos) / len(pos)
    return min(range(len(pos)), key=lambda i: (pos[i][0]-cx)**2 + (pos[i][1]-cy)**2)


def measure(name, group, pos, env, terrain=None, talker=0, slots=4000, note=""):
    sim = Simulation(Static(pos), env, BUD, talker=talker, terrain=terrain)
    settle = CONFIG.beacon_interval_slots * 4
    sim.run(settle)
    sim.run(slots - settle, voice_from=settle)
    d = sim.delivery(sim.nodes[talker].addr)
    lat = sim.latency_slots(sim.nodes[talker].addr)
    hops = [int(round(lat[i])) + 1 for i in lat if i != talker]
    ch = sim.channel
    links = [[i, j] for i in range(len(pos)) for j in range(i + 1, len(pos))
             if ch.rx_dbm_between(pos[i], pos[j]) >= BUD.sensitivity_dbm]
    # How far the voice actually got, end to end -- as against how far one hop reaches.
    #
    # USABLE reach, not "somebody heard something". A radio getting 40% of payloads is not
    # in the conversation; it is hearing every other word. The threshold is 90%, which is
    # roughly where speech stops sounding broken, and radios past it are counted as out of
    # contact even when the map shows a line to them.
    USABLE = 0.90
    tx, ty = pos[talker]
    dist = lambda i: ((pos[i][0]-tx)**2 + (pos[i][1]-ty)**2) ** 0.5
    heard = [i for i in range(len(pos)) if d.get(i, 0.0) >= USABLE]
    xs_all = [q[0] for q in pos]; ys_all = [q[1] for q in pos]
    return {
        "name": name, "group": group, "note": note,
        # Transmitter to the LAST RECEIVING radio -- a radius from whoever is speaking,
        # deliberately not a network diameter. Two radios on opposite edges both hear the
        # talker, but nothing here shows they can hear EACH OTHER, so quoting an
        # edge-to-edge span would imply a link that has never been measured.
        "reach_m": round(max((dist(i) for i in heard), default=0.0)),
        "usable": len(heard), "usable_pct": round(len(heard) / len(pos) * 100),
        "extent_m": round(max(max(xs_all)-min(xs_all), max(ys_all)-min(ys_all))),
        "hop_m": round(usable_range_m(env, BUD)),
        "env": env.name, "nodes": len(pos), "talker": talker,
        "horizon_m": round(usable_range_m(env, BUD)),
        "pos": [[round(x), round(y)] for x, y in pos],
        "links": links,
        "delivery": [round(d.get(i, 0.0) * 100, 1) for i in range(len(pos))],
        "worst": round(min(d.values()) * 100, 1),
        "mean": round(sum(d.values()) / len(d) * 100, 1),
        "reachable": sim.reachable_from(talker),
        "max_hops": max(hops) if hops else 0,
        "relays": sim.relay_total(),
        "terrain": ([[round(terrain.crest_x), round(terrain.height_m)]]
                    if isinstance(terrain, Ridge) else []),
    }


def build():
    out = []
    Rw = horizon_m(WOOD, BUD)
    one = within_one_hop_m(WOOD, BUD)

    # ---- single units -------------------------------------------------------
    out.append(measure("Two radios, in range", "Single units",
                       [(0.0, 0.0), (one, 0.0)], WOOD,
                       note="The simplest case. No relaying, nothing to decide."))
    out.append(measure("Two radios, out of range", "Single units",
                       [(0.0, 0.0), (Rw * 1.4, 0.0)], WOOD,
                       note="Beyond the horizon with nobody between. Nothing can help."))
    out.append(measure("Two radios, one relay between", "Single units",
                       [(0.0, 0.0), (one, 0.0), (2 * one, 0.0)], WOOD,
                       note="One radio in the middle turns a dead link into a working one."))

    # ---- clusters -----------------------------------------------------------
    out.append(measure("Twelve together", "Clusters",
                       [(SHOULDER * (i % 4), SHOULDER * (i // 4)) for i in range(12)], WOOD,
                       note="Everyone hears everyone. Correct behaviour is zero relaying."))
    out.append(measure("Two clusters, a gap between", "Clusters",
                       [(SHOULDER * (i % 3), SHOULDER * (i // 3)) for i in range(6)]
                       + [(one * 1.6 + SHOULDER * (i % 3), SHOULDER * (i // 3)) for i in range(6)],
                       WOOD, note="Two groups just far enough apart to need each other."))
    out.append(measure("Cluster — single — cluster", "Clusters",
                       [(SHOULDER * (i % 3), SHOULDER * (i // 3)) for i in range(6)]
                       + [(one * 1.5, PAIR)]
                       + [(one * 3.0 + SHOULDER * (i % 3), SHOULDER * (i // 3)) for i in range(6)],
                       WOOD, note="One person carries the whole link between two groups."))
    out.append(measure("Single — cluster — single", "Clusters",
                       [(0.0, 0.0)]
                       + [(one + SHOULDER * (i % 3), SHOULDER * (i // 3)) for i in range(6)]
                       + [(one * 2.4, 0.0)], WOOD,
                       note="A group in the middle relaying for two lone walkers."))

    # ---- dispersed ----------------------------------------------------------
    out.append(measure("Twelve strung out in a line", "Dispersed",
                       [(i * one, 0.0) for i in range(12)], WOOD,
                       note="Evenly spread. Every radio is a relay for the next."))
    out.append(measure("Four groups along a valley", "Dispersed",
                       [(g * one * 1.4 + ARMS_LENGTH * (i % 3), ARMS_LENGTH * (i // 3))
                        for g in range(4) for i in range(4)], WOOD,
                       note="Clusters of four, each within reach of the next."))

    # ---- terrain ------------------------------------------------------------
    out.append(measure("The repeater triangle", "Terrain",
                       [(c + (i - 2) * NEAR, (i % 2) * STAGGER)
                        for c in (0.0, one, 2 * one) for i in range(4)], WOOD,
                       terrain=Ridge(crest_x=one, height_m=80.0, width_m=400.0),
                       note="Two valleys and a hilltop. The case the product exists for."))
    out.append(measure("The same hill, nobody on top", "Terrain",
                       [(c + (i - 2) * NEAR, (i % 2) * STAGGER)
                        for c in (0.0, 2 * one) for i in range(4)], WOOD,
                       terrain=Ridge(crest_x=one, height_m=80.0, width_m=400.0),
                       note="Remove the hilltop group and the network severs. The control."))
    out.append(measure("Mountain, 200 m, groups either side", "Terrain",
                       [(c + (i - 2) * NEAR, (i % 2) * STAGGER)
                        for c in (0.0, one * 1.2, one * 2.4) for i in range(4)], WOOD,
                       terrain=Ridge(crest_x=one * 1.2, height_m=200.0, width_m=900.0),
                       note="A serious obstruction. Only the party on the summit links them."))

    # ---- environments -------------------------------------------------------
    Ro = horizon_m(OPEN, BUD)
    out.append(measure("Open moorland, twelve in a line", "Environment",
                       [(i * Ro * 0.55, 0.0) for i in range(12)], OPEN,
                       note="Same shape as the woodland line, longer links."))
    d350 = dense(THICK_HORIZON)
    out.append(measure("Thick woodland, twelve over 2 km", "Environment",
                       [(i * SPREAD_SPLIT / 11.0, 0.0) for i in range(12)], d350,
                       note="Short hops. All twelve connected; the hop budget runs out."))
    out.append(measure("Thick woodland, twelve over 1 km", "Environment",
                       [(i * SPREAD_OK / 11.0, 0.0) for i in range(12)], d350,
                       note="Same cover, group kept tighter. Works."))

    # ---- who is talking -----------------------------------------------------
    # Every scenario above has the talker at one END. Real use is usually somebody in the
    # MIDDLE, and that halves the hops needed in each direction.
    line12 = [(i * one, 0.0) for i in range(12)]
    out.append(measure("Line of twelve, talker at the end", "Who is talking",
                       line12, WOOD, talker=0,
                       note="Eleven hops from end to end. TTL allows seven."))
    out.append(measure("Line of twelve, talker in the middle", "Who is talking",
                       line12, WOOD, talker=centre_of(line12),
                       note="Same twelve radios, same ground. Six hops each way, not eleven."))
    thick = dense(THICK_HORIZON)
    thickline = [(i * SPREAD_SPLIT / 11.0, 0.0) for i in range(12)]
    out.append(measure("Thick woodland 2 km, talker at the end", "Who is talking",
                       thickline, thick, talker=0,
                       note="The case that splits the group."))
    out.append(measure("Thick woodland 2 km, talker in the middle", "Who is talking",
                       thickline, thick, talker=centre_of(thickline),
                       note="The same group and the same cover, spoken from the centre."))

    # ---- 2D meshes ----------------------------------------------------------
    # A real group does not form a queue. Several neighbours each, more than one route
    # between any two people -- the shape every MANET diagram shows and no line reproduces.
    for n, ext, seed, who in ((20, one*3.2, 7, "centre"), (20, one*3.2, 7, "edge"),
                              (40, one*4.6, 11, "centre"), (60, one*5.4, 3, "centre")):
        pts = scatter(n, ext, seed)
        t = centre_of(pts) if who == "centre" else min(range(n), key=lambda i: pts[i][0])
        out.append(measure(
            f"{n} radios spread over ground, talker at the {who}", "Real dispersal",
            pts, WOOD, talker=t, slots=3000,
            note=("Multiple routes between any two radios — the redundancy a line has not got."
                  if who == "centre" else
                  "The same scattered group, spoken from one corner instead of the middle.")))
    pts = scatter(30, one*4.0, seed=5)
    out.append(measure("30 radios over a ridge, talker centre", "Real dispersal",
                       pts, WOOD, talker=centre_of(pts),
                       terrain=Ridge(crest_x=one*2.0, height_m=80.0, width_m=400.0),
                       slots=3000,
                       note="Scattered across ground with a hill through the middle of it."))

    # ---- alternating --------------------------------------------------------
    alt = []
    for k in range(2):
        alt.append((k * one * 3.0, 0.0))                                    # a lone walker
        alt += [(k * one * 3.0 + one * 1.5 + SHOULDER * (i % 3),
                 SHOULDER * (i // 3)) for i in range(5)]                    # then a group
    alt.append((2 * one * 3.0, 0.0))
    out.append(measure("Single — cluster — single — cluster — single", "Clusters",
                       alt, WOOD, talker=0,
                       note="Lone walkers and groups alternating along a route."))
    out.append(measure("The same, spoken from the middle group", "Clusters",
                       alt, WOOD, talker=centre_of(alt),
                       note="Identical radios. Only who pressed the button has changed."))

    # ---- at the ceiling -----------------------------------------------------
    # Nothing above reaches the seven hops the latency budget allows. These do, and they
    # show what the edge of the design actually looks like rather than the middle of it.
    for n, mult, seed in ((60, 5.5, 13), (100, 8.5, 13)):
        pts = scatter(n, Rw*mult, seed)
        out.append(measure(f"{n} radios over {Rw*mult/1000:.0f} km, talker centre",
                           "At the ceiling", pts, WOOD, talker=centre_of(pts), slots=2500,
                           note="Spread until the hop budget is the thing that stops it."))
    # SEVEN groups, not eight. An even number has no middle group: the talker lands in
    # group 3 of 0-7 with three groups behind it and four in front, so one side is a hop
    # deeper and the pair reads as an asymmetry in the protocol when it is an asymmetry in
    # the arrangement. Seven gives a true centre and makes the two cards comparable.
    big = [(g * one * 1.05 + SHOULDER * (i % 3), SHOULDER * (i // 3))
           for g in range(7) for i in range(6)]
    out.append(measure("Seven groups of six, end to end", "At the ceiling",
                       big, WOOD, talker=0, slots=3000,
                       note="42 radios in seven clusters. Spoken from the far end."))
    out.append(measure("Seven groups of six, spoken from the middle", "At the ceiling",
                       big, WOOD, talker=centre_of(big), slots=3000,
                       note="The same 42 radios, three groups either side. Half the hops."))

    # ---- scale --------------------------------------------------------------
    for n in (32, 100):
        side = int(n ** 0.5) + 1
        step = one / (side * 1.2)
        out.append(measure(f"{n} radios, dense cluster", "Scale",
                           [((i % side) * step, (i // side) * step) for i in range(n)], WOOD,
                           slots=2500,
                           note="Everyone in earshot. Tests the neighbour tables, not reach."))
    out.append(measure("32 radios, eight groups over a ridge", "Scale",
                       [(g * one * 0.9 + PAIR * (i % 2), ROW * (i // 2))
                        for g in range(8) for i in range(4)], WOOD,
                       terrain=Ridge(crest_x=one * 3.6, height_m=80.0, width_m=400.0),
                       slots=3000, note="Scale and terrain together."))
    return out


if __name__ == "__main__":
    data = build()
    print(json.dumps({"config": {
        "slot_ms": CONFIG.slot_us / 1000, "slots_per_frame": CONFIG.slots_per_frame,
        "voice_ttl": CONFIG.voice_ttl, "bitrate": CONFIG.gross_bitrate,
    }, "scenarios": data}, indent=1))
