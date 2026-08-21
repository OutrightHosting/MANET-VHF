# Acquisition preamble budget

**The number the reach of this product hangs on.** At 154 bits the network is three hops;
at 56 it is four with three times the error correction, and with one more lever it is
twelve. Established here from primary sources; settled properly on a bench.

## Line item budget

At 19.2 kbps gross the symbol rate is 9600 sym/s. **The CC1200 sends preamble and sync word
as 2-GFSK even when the payload is 4-GFSK** (SWRU346B §5.2.1), so each transmitted
preamble/sync bit costs one symbol — **two payload bit-times**. `MANET_SYNC_BITS` is a time
budget in bit-times, not a count of sync-word bits.

| Item | Bit-times | Removed by GPS? | Source |
|---|---|---|---|
| RX-chain settling after SRX | ~34 | **Yes** — strobe SRX in the guard, not at the slot edge | SWRS123D §4.13 |
| AGC gain settling | **8** | No | SWRU346B §6.6 — "4 bits preamble needed for AGC settling" |
| Bit/symbol timing acquisition | **0** with `TOC_LIMIT=0` | Yes | SWRU346B Table 19 |
| Frequency-offset compensation | **0** with `TOC_LIMIT=0` | **Only if the 40 MHz reference is disciplined** | SWRU346B §6.6 |
| Preamble qualifier (PQT) | 0 with `PQT_EN=0` | Yes | SWRU346B §6.8 |
| Sync word (11/16/18/24/32 symbols) | **48** at 24 bits | Partly — gating shrinks it ~8–10 bits, not to zero | SWRU346B §6.7 |
| Mode/rate signalling | **0** — single-mode PHY | n/a | — |
| PA ramp, turnaround, propagation | ~14 | No — **guard item, not preamble** | SWRU346B §7.1 |

**Total: 8 + 48 = 56 bit-times = 2.9 ms.**

Sync word length is set by false-alarm rate, not by taste. Over a gated ±2-symbol window,
16-bit gives roughly one false sync every 21 s at a 200 ms frame — marginal. 24-bit gives
one per ~5 minutes. **That is why 56 and not 40.**

**24 bit-times is not reachable on this silicon.** It is 12 transmitted bits, below the
11-bit minimum sync word with no preamble at all.

| | Value | Condition |
|---|---|---|
| Best | 30 | 11-bit sync, AGC pre-set per neighbour. False-sync ~1/s — unlikely to survive a bench |
| **Likely** | **40–56** | GPS-disciplined **LO**, `TOC_LIMIT=0`, PQT and carrier-sense gating off |
| Worst | 128 | LO free-running at ±2 ppm → `TOC_LIMIT ≥ 1` → 2–4 bytes of preamble |

## Why NBWF needs 8 ms and we do not

Read from the source, not inferred:

1. **It is not a measurement.** §4.3 states `T1 = 8 ms` with footnote 4 reading, in full,
   *"This is an estimate."* The fields §2.3 specifies sum to **5.3 ms**.
2. **It is a lump allowance.** The same paragraph computes 22.5 − 8 = 14.5 ms with no guard,
   ramp or turnaround deducted anywhere. Importing 154 bits *and* charging
   `MANET_GUARD_PERMILLE` separately **double-counts about 2.7 ms per burst**.
3. **1.7 ms is architecturally inapplicable.** The Par field carries *"data rate, interleaver
   length, burst length"* for a five-mode PHY; the transition field exists *"because of a
   change in symbol rate"*. We have one mode and one symbol rate.
4. **The 1.5 ms sync preamble is a CW tone for frequency**, and §4.11 explains why they
   cannot delete it: *"the NBWF shall not be dependent on the availability of such external
   signals [GNSS]"*, with nodes synchronised only *"to within 1.5 msec"*. We are GPS-locked,
   1500× tighter.

**The qualification that matters:** GPS *slot timing* is worth only ~8–10 bits, all on the
sync word. The other ~90 come from the disciplined *frequency reference* and from being
single-mode. **If the CC1200's 40 MHz reference free-runs, NBWF's figure is roughly right
and we land near 128.** GPS-disciplining the LO is therefore a design commitment, not an
assumption — see [OQ-0003](open-questions.md#oq-0003).

## The bench tests that settle it

Two CC1200 boards at 155 MHz — note the datasheet lists 137–158.3 MHz as *"possible support
for additional frequency bands"*, **not a characterised band**, so expect a custom match.
Baseline: `TOC_LIMIT=0`, `FOC_EN=0`, `PQT_EN=0`, `CARRIER_SENSE_GATE=0`, `FS_AUTOCAL=00b`.

| Test | Measure | Confirms 56 | Refutes → 128 |
|---|---|---|---|
| **A. Minimum preamble** | PER vs `NUM_PREAMBLE`, **with a 60–90 dB level step between consecutive bursts** — the real case, a near relay then a far originator | 4 bits clean | needs ≥ 8 |
| **B. Frequency tolerance** — the decisive one | PER vs injected offset ±0–1 kHz at `FOC_EN=0` | flat to ≥ ±200 Hz | narrower than ±150 Hz |
| **C. False sync on noise** | 50 Ω terminated, count `SYNC_EVENT` over 1 h across `SYNC_MODE` | 24-bit < 10⁻²/s | 24-bit fails → 32-bit, +16 |
| **D. RX-entry latency** | Scope SRX → `RSSI_VALID` at 26 kHz RX BW | ≤ guard interval | > guard → settling re-enters the burst |
| **E. Relay turnaround** | Scope STX → PA_PD, including STM32F4 SPI handling | ≈ 43 µs + firmware | > 2 ms → slot floor, not preamble, binds |

Test B is the one to run first. It decides between 56 and 128, and therefore between a
four-hop network and a three-hop one.
