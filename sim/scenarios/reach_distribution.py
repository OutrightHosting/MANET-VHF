"""
Reach as a distribution, because a single number implies a certainty the physics has not
got. M-03.

Two radios the same distance apart in the same woodland differ by several dB depending on
what happens to be between them. FFI quote NBWF's range as median, 10% and 90%; we had been
quoting one number.

The shadowing figure is not borrowed. It is DERIVED from FFI's own published quantiles for
mode N1 -- 22.0 km median, 13.1 km at 90%, 36.9 km at 10%, under Egli with exponent 4 --
and both sides independently give sigma = 7.0 dB, which is the check that it is a real
parameter rather than a rounded ratio.

  python3 -m sim.scenarios.reach_distribution
"""
import random

from ..manet.radio import ENVIRONMENTS, LinkBudget, usable_range_m, usable_range_quantiles

WOOD = ENVIRONMENTS["woodland"]
BUD = LinkBudget()
SEED = 20260822          # geometry-exempt: a seed, not a distance
TRIALS = 20000           # geometry-exempt: a sample count, not a distance


def hop_length_m(rng, env, budget):
    """One hop's reach with log-normal shadowing applied."""
    median = usable_range_m(env, budget)
    fade_db = rng.gauss(0.0, budget.shadowing_db)
    return median * 10.0 ** (-fade_db / (10.0 * env.exponent))


def chain_reach(hops, env=WOOD, budget=BUD, trials=TRIALS, seed=SEED):
    """(p90, median, p10) total reach over `hops` independently-faded hops."""
    rng = random.Random(seed)
    totals = sorted(sum(hop_length_m(rng, env, budget) for _ in range(hops))
                    for _ in range(trials))
    return (totals[int(trials * 0.10)],
            totals[int(trials * 0.50)],
            totals[int(trials * 0.90)])


if __name__ == "__main__":
    lo, med, hi = usable_range_quantiles(WOOD, BUD)
    print(f"single hop, woodland: {lo/1000:.1f} / {med/1000:.1f} / {hi/1000:.1f} km "
          f"(9-in-10 / median / 1-in-10), sigma {BUD.shadowing_db:.1f} dB\n")
    print(f"{'hops':>5} {'9 in 10 reach':>15} {'median':>10} {'1 in 10 reach':>15} {'spread':>8}")
    for n in (1, 2, 4, 7):
        p90, m, p10 = chain_reach(n)
        print(f"{n:>5} {p90/1000:>12.1f} km {m/1000:>7.1f} km {p10/1000:>12.1f} km "
              f"{p10/p90:>7.1f}x")
    print()
    print("The spread NARROWS with hop count. Independent shadowing on each hop partly")
    print("averages out, so a seven-hop path is more predictable than a one-hop link:")
    print("3.9x uncertainty becomes 1.7x. The mesh does not only extend reach, it makes")
    print("reach less of a lottery -- which is an argument for it that we did not have,")
    print("and a real counterweight to OQ-0030's case for fewer, longer hops.")
    print()
    print("NOTE these are reaches at the radio horizon, so they are an upper bound. A")
    print("chain at practical spacing sits well inside them: the measured 7-hop chain is")
    print("18.2 km at 2.6 km spacing, against the 34.9 km median here.")
