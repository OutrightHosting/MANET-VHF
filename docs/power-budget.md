# Power budget — the 10–12 hour requirement

**Requirement (2026-08-21): 10–12 hours of operation.** A leader collects a radio at the start
of an event and hands it back at the end. Mid-day charging is not available and swapping
batteries in the field is not a plan.

## The 3.5 hour figure was wrong, and how

[OQ-0026](open-questions.md#oq-0026) reported 4.7–11.7 h, and the barrage stress test pushed
the worst case to 3.5 h. Both rested on two numbers with nothing behind them:

1. **Baseband, GPS and audio at 1.5 W.** Invented. Not measured, not sourced, not budgeted.
2. **The talker keys PTT 100% of the time.** The simulator's talker never releases. Every
   duty-cycle figure in this project is therefore *per unit of channel occupancy*, and was
   read as though it were per unit of wall-clock time.

Worst-node TX duty is 20.4% (one talker) / 29.3% (two, under barrage) **at 100% occupancy**.
Nobody talks for ten hours. Scaled to real occupancy, with the beacon floor of 0.36% that
persists whether anyone talks or not:

| Channel occupancy | Worst-node TX duty | PA average @ 25% | @ 40% | @ 55% |
|---|---|---|---|---|
| 100% (the old figure) | 29.3% | 5.86 W | 3.66 W | 2.66 W |
| 30% — a very heavy day | 9.0% | 1.81 W | 1.13 W | 0.82 W |
| 15% — busy | 4.7% | 0.94 W | 0.59 W | 0.43 W |
| 8% — normal | 2.7% | 0.54 W | 0.33 W | 0.24 W |

## Runtime against the requirement

4S 3.0 Ah Li-ion, 44 Wh, two concurrent talkers (the worse case):

| Occupancy | Baseband | PA η | Total | Runtime |
|---|---|---|---|---|
| **30%** | **1.5 W** | **25%** | **3.31 W** | **13.3 h** |
| 30% | 0.6 W | 55% | 1.42 W | 30.9 h |
| 15% | 1.5 W | 25% | 2.44 W | 18.0 h |
| 8% | 0.6 W | 55% | 0.84 W | 52.2 h |

**Every row meets 12 hours.** The top row is deliberately the worst credible combination —
sustained 30% channel occupancy across a whole day, a cheap 25%-efficient PA module, and the
invented 1.5 W baseband — and it still gives 13.3 h.

## What this changes about the design

**The transmit path is no longer the constraint. The always-on receive path is.** At 8–15%
occupancy the PA contributes 0.24–0.94 W while baseband contributes 0.6–1.5 W. Halving PA
draw buys minutes; halving baseband draw buys hours.

That inverts the effort. Priorities, in order:

1. **Measure the always-on power** — CC1120/CC1200 in RX, the STM32F4 running the core and
   Codec2, the GPS receiver, and the audio path. This is now the single most important
   number in the power budget and it is currently a guess. Phase 1 bench.
2. **Duty-cycle the GPS.** A fix every few minutes with holdover between
   ([OQ-0031](open-questions.md#oq-0031)) removes most of a continuously-tracking receiver's
   draw. This has to be designed alongside holdover, not bolted on.
3. **Clock the MCU down.** Codec2 3200 does not need 168 MHz. The core is freestanding, no
   malloc, no float ([ADR-0006](decisions/0006-c-core-python-harness.md)), so it will run at
   whatever clock the vocoder demands and no more.
4. **PA efficiency still matters, but less than assumed.** 4FSK is constant-envelope, so the
   PA can run saturated and 45–55% is a design target rather than a hope. Worth doing, but it
   is not what buys the ten hours.

## What could still break it

- **Baseband above ~2.5 W.** Then even at low occupancy the budget fails on standby alone.
  This is the number to measure first.
- **Sustained occupancy above ~50%.** Not a normal day, but a prolonged incident with
  continuous traffic. Worth knowing the number rather than discovering it.
- **A larger network.** Every duty figure here is 12 nodes. Relay load per node under barrage
  is roughly flat with network size (everyone who hears, relays), but the *channel* fills up
  — so occupancy rises with node count even if per-node duty does not.
- **Thermal, separately from battery.** 5.86 W of PA draw dissipating ~4.4 W inside a plastic
  case is a thermal problem at 100% occupancy even though it is not a battery problem at 15%.
  Sustained transmit needs a junction-temperature measurement regardless of runtime.
