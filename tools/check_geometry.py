#!/usr/bin/env python3
"""
Reject bare distance literals in simulation scenarios.

Three scenarios shipped with distances hardcoded in metres against a radio horizon that
later moved, and all three kept reporting PASS while testing nothing (see
sim/manet/geometry.py). This is the guard that stops a fourth.

A scenario states distances as INTENT and derives metres from the measured horizon. Numbers
that are not radio distances -- times, durations, antenna heights, ridge dimensions, cluster
jitter -- are exempt, by name, on the line where they appear.

  make geometry-check
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SCENARIOS = sorted((ROOT / "sim" / "scenarios").glob("*.py"))

# Anything at or above this, written as a float literal, is assumed to be metres until
# proven otherwise. Below it we would drown in indices and small factors.
SUSPICIOUS = 100.0

# Keywords that make a literal something other than a radio distance. Matched against the
# whole source line, so `period_s=240.0` and `height_m=80.0` pass without ceremony.
EXEMPT_ON_LINE = (
    "_s=", "_us", "_ms", "seconds", "slots", "period", "duration", "timeout",
    "height_m", "width_m", "crest", "jitter", "cluster_m", "freq", "hz",
    "dbm", "_db", "watt", "percent", "_pct", "wh", "mah",
    "geometry-exempt",          # explicit escape hatch, must say why on the same line
)

LITERAL = re.compile(r"(?<![\w.])(\d+\.\d+)")


def offenders(path):
    out = []
    for n, line in enumerate(path.read_text().splitlines(), 1):
        code = line.split("#", 1)[0]
        if not code.strip():
            continue
        low = line.lower()
        if any(k in low for k in EXEMPT_ON_LINE):
            continue
        for lit in LITERAL.findall(code):
            if float(lit) >= SUSPICIOUS:
                out.append((n, lit, line.strip()))
    return out


def main():
    bad = {p: o for p in SCENARIOS if (o := offenders(p))}
    if not bad:
        print(f"ok: no bare distance literals in {len(SCENARIOS)} scenario files")
        return 0
    print("BARE DISTANCE LITERALS — a scenario is pinning a distance in metres against a")
    print("radio horizon that moves. This is how gate Q1, gate Q5 and hill.py each spent")
    print("weeks reporting PASS while testing nothing.\n")
    for path, items in bad.items():
        print(f"  {path.relative_to(ROOT)}")
        for n, lit, line in items:
            print(f"    :{n}  {lit}   {line[:88]}")
    print("\nUse sim/manet/geometry.py — hop_span_m, within_one_hop_m, severed_m,")
    print("chain_spacing_m — or, if it genuinely is not a radio distance, say so on the")
    print("line with a name the checker knows or a `geometry-exempt:` comment giving the")
    print("reason.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
