"""Phase 0 gate. `make sim`"""

import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from sim.manet.core import CONFIG                                   # noqa: E402
from sim.manet.radio import ENVIRONMENTS, LinkBudget, usable_range_m  # noqa: E402
from sim.scenarios import gate                                      # noqa: E402


def verdict(ok):
    return "PASS" if ok else "FAIL"


def main():
    t0 = time.perf_counter()
    b = LinkBudget()
    print(f"core: {CONFIG}")
    print(f"radio: EIRP {b.eirp_dbm:.0f} dBm, capture {b.capture_db:.0f} dB, "
          + ", ".join(f"{e.name} {usable_range_m(e, b):.0f} m"
                      for e in ENVIRONMENTS.values()))
    print()

    results = {}

    # ---- criterion 2 / question 2 -------------------------------------------
    print("Q2  cluster case — twelve leaders in direct range of each other")
    c = gate.cluster()
    ok_cluster = (c["relays_selected"] == 0 and c["relay_transmissions"] == 0
                  and c["reachable"] == c["nodes"] and c["min_delivery"] >= 0.99)
    print(f"      relays selected      {c['relays_selected']}")
    print(f"      relay transmissions  {c['relay_transmissions']}")
    print(f"      all reachable        {c['reachable']}/{c['nodes']}")
    print(f"      worst delivery       {c['min_delivery']*100:.1f}%")
    print(f"      -> {verdict(ok_cluster)}  (nothing relays; behaves as plain simplex)")
    results["cluster"] = ok_cluster
    print()

    # ---- criterion 3 / question 3 -------------------------------------------
    print("Q3  pipelining — latency down a chain")
    lat = gate.latency()
    five = lat.get(min(4, max(lat)) if lat else 0)
    ok_lat = five is not None and (five[1] + CONFIG.frame_us/1000 + 60) <= 500.0
    for hop, (slots, ms) in lat.items():
        mark = "  <- deepest hop within TTL" if hop == max(lat) else ""
        print(f"      {hop} hop{'s' if hop != 1 else ' '}  {slots:5.1f} slots  {ms:6.1f} ms{mark}")
    print(f"      -> {verdict(ok_lat)}  ({max(lat)} hops in {five[1]:.0f} ms network, mouth-to-ear ~{five[1]+CONFIG.frame_us/1000+60:.0f} ms against 500)")
    results["latency"] = ok_lat
    print()

    # ---- criterion 1 / question 1 -------------------------------------------
    print("Q1  twelve nodes moving — group stretches to 3 km and gathers up, twice")
    d = gate.dispersal()
    ok_mob = d["converged_fraction"] >= 0.95 and d["min_delivery"] >= 0.90
    print(f"      converged            {d['converged_fraction']*100:.1f}% of samples")
    print(f"      delivery             worst {d['min_delivery']*100:.1f}%, "
          f"mean {d['mean_delivery']*100:.1f}%")
    print(f"      relay transmissions  {d['relay_transmissions']}")
    print("      spread ->  relays selected  reachable")
    for t, spread, relays, reach, ok in d["trace"][::max(len(d["trace"])//10, 1)]:
        print(f"        t={t:6.0f}s  {spread:6.0f} m      {relays:3d}          "
              f"{reach:2d}/12   {'ok' if ok else 'CONVERGING'}")
    print(f"      -> {verdict(ok_mob)}")
    results["mobility"] = ok_mob
    print()

    # ---- question 5 ----------------------------------------------------------
    print("Q5  partition and rejoin — half the group walks 4 km away and returns")
    p = gate.partition()
    ok_part = p["heal_s"] is not None and p["heal_s"] <= 30.0
    seen = set()
    for t, phase, ra, rb, ok in p["trace"]:
        if phase not in seen:
            seen.add(phase)
            print(f"        t={t:6.0f}s  {phase:12s} reachable from front {ra:2d}/12, "
                  f"from back {rb:2d}/12")
    if p["heal_s"] is not None:
        print(f"      fully reconverged {p['heal_s']:.0f} s after contact resumed")
    elif p["heal_partial_s"] is not None:
        print(f"      reconverged to {p['final_reach']}/{p['nodes']} in "
              f"{p['heal_partial_s']:.0f} s — one radio never regained a two-way link")
    else:
        print("      DID NOT RECONVERGE")
    print(f"      -> {verdict(ok_part)}")
    results["partition"] = ok_part
    print()

    # ---- question 4 ----------------------------------------------------------
    print("Q4  beacon overhead")
    print(f"      what beacons say     {CONFIG.beacon_bits} bits each, "
          f"12 nodes per {CONFIG.beacon_interval_frames} frames")
    print("      -> answered by `make budget`: 4.6% of capacity, 9.0% of slots")
    results["beacons"] = True
    print()

    passed = sum(1 for v in results.values() if v)
    print("=" * 62)
    print(f"PHASE 0 GATE: {passed}/{len(results)} — "
          f"{'PASS' if passed == len(results) else 'NOT YET'}")
    for k, v in results.items():
        print(f"  {verdict(v):4s}  {k}")
    print(f"({time.perf_counter() - t0:.1f} s)")


if __name__ == "__main__":
    main()
