"""Phase 0 experiments. `make sim`"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from sim.manet.core import CONFIG                   # noqa: E402
from sim.manet.radio import ENVIRONMENTS, LinkBudget, usable_range_m  # noqa: E402
from sim.scenarios import reuse                     # noqa: E402


def main():
    print(f"core built as: {CONFIG}")
    b = LinkBudget()
    print(f"link budget:   EIRP {b.eirp_dbm:.0f} dBm, max path loss "
          f"{b.max_path_loss_db:.0f} dB, capture margin {b.capture_db:.0f} dB")
    for name, env in ENVIRONMENTS.items():
        print(f"  {env.name:16s} usable range {usable_range_m(env, b):7.0f} m")

    print()
    print("OQ-0013 — spatial reuse in an 8-node chain")
    print(f"{'environment':16s} {'spacing':>9s} {'C/I':>7s} {'conv':>5s} "
          f"{'relays':>7s} {'lost':>6s} {'end-to-end delivery':>20s}")
    print("-" * 78)

    for name in ("woodland", "open"):
        r = reuse.run(env_name=name)
        d = r["delivery"]
        last = max(d) if d else 0
        print(f"{r['env']:16s} {r['spacing_m']:8.0f}m {r['margin_db']:6.1f}dB "
              f"{str(r['converged']):>5s} {r['relays']:7d} {r['collisions']:6d} "
              f"{d.get(last, 0.0) * 100:18.1f}%")

    print()
    print("per-hop delivery (fraction of the talker's payloads decoded):")
    for name in ("woodland", "open"):
        r = reuse.run(env_name=name)
        row = " ".join(f"{r['delivery'].get(i, 0.0) * 100:5.0f}" for i in sorted(r["delivery"]))
        print(f"  {r['env']:16s} {row}")


if __name__ == "__main__":
    main()
