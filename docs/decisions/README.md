# Decision log

Architecture decisions for the narrowband voice MANET. One file per decision, numbered in the
order taken, never renumbered.

Most of ADR-0001 through ADR-0005 record decisions that were already made before this repository
existed — they are documented in [the engineering brief](../engineering-brief.md) §4 and §5 as
"decided". ADR-0007 records [Addendum 01](../addendum-01-packet-architecture.md). Writing them up here is not re-opening them. It is recording *why*, so that when Phase 0
produces a result that contradicts one, we can tell the difference between a decision that was
wrong and a decision whose premise changed.

Every ADR carries a **Reversal trigger**: the specific observation that would require re-opening it.
If Phase 0 produces that observation, the ADR gets a superseding successor. If it does not, the
decision stands and we stop discussing it.

| ADR | Decision | Status |
|---|---|---|
| [0001](0001-narrowband-vhf-licensed-spectrum.md) | Narrowband VHF on licensed UK Business Radio spectrum | Accepted |
| [0002](0002-tdma-slot-pipelining.md) | 4-slot TDMA with slot pipelining, not store-and-forward | Accepted |
| [0003](0003-olsr-mpr-routing.md) | OLSR-derived proactive routing with MPR flooding | Accepted |
| [0004](0004-codec2-3200.md) | Codec2 at 3200 bps as the vocoder | Accepted |
| [0005](0005-cc1200-stm32-bench-platform.md) | CC1200 + STM32F4 bench platform; SDR and adapted DMR rejected | **Re-opened** — no CC1200 EM exists in band |
| [0006](0006-c-core-python-harness.md) | One protocol implementation in C, driven by a Python simulation harness | Accepted |
| [0007](0007-packet-switched-frame-architecture.md) | Packet-switched frame architecture with typed, addressed frames | Accepted |
| [0008](0008-four-slots.md) | ~~Four slots per frame — forced from both directions~~ | Superseded by 0009 |
| [0009](0009-frame-structure-with-real-preamble.md) | Frame re-derived once the preamble was measured | Accepted — **but its figures are stale, see note below** |
| [0010](0010-terrain-diffraction.md) | Terrain per-link by knife-edge diffraction, not as an environment | Accepted |
| [0011](0011-barrage-relaying.md) | Barrage relaying: identical concurrent copies combine, election removed for voice | Accepted |
| [0012](0012-network-time-authoritative.md) | The network is the clock; GPS is advisory and sanity-checked | Accepted (design) |
| [0013](0013-what-we-take-from-vine.md) | Take VINE's header-inspection routing; leave its MAC and power model | Accepted |
| [0014](0014-reserved-signalling-slots.md) | One slot in eight reserved for signalling; voice steps over it | Accepted |

## Two warnings before you read any of these

**1. The live frame is not stated in any accepted ADR.** ADR-0009 set the frame, its own
reversal trigger then fired when the preamble was re-derived from CC1200 primary sources
([preamble-budget.md](../preamble-budget.md)), the frame moved with it — and no superseding ADR
was ever written. The only correct statement of the live frame anywhere in this directory sits
inside **ADR-0008, which is marked Superseded**, in a correction note retrofitted to its
Consequences section. A reader who obeys the banner and skips the file will miss it.

The live values are **160 ms frame, four slots of 40 ms**, pinned by
`core/tests/test_config.c:23`. Take them from `core/include/manet/config.h`, not from here.

**2. Several ADRs are amended by later ones without saying so.** The `Amends / Amended by`
field in the format below is not reflected in the table above, and the headers are incomplete:

| ADR | is also amended by | in what way |
|---|---|---|
| 0002 | 0007, 0008, 0011, 0014 | pipelining decision is **live**; every number in it is stale |
| 0003 | 0011 | MPR still selects and still fills beacon adverts; it no longer decides who forwards **voice** |
| 0004 | 0009 | Codec2 3200 is live; the ~47% FEC figure is dead, the build runs 16% |
| 0007 | OQ-0018 | a seventh header field (`prev`) was added; the header is 42 bits, not 34 |
| 0009 | 0014, preamble-budget.md | frame and per-hop figures superseded |
| 0011 | 0014 | NAMA still owned beacons and nothing arbitrated them against voice — that was B-15 |

For a single narrative that is checked against the code rather than against other documents,
read **[how-it-works.md](../how-it-works.md)**.

## Format

```
# ADR-NNNN: Title

Status:  Proposed | Accepted | Superseded by ADR-NNNN
Amends / Amended by: ADR-NNNN (optional)
Date:    YYYY-MM-DD
Phase:   Pre-development | Phase 0 | Phase 1 | ...

## Context
## Decision
## Consequences
## Reversal trigger
## Alternatives rejected
```
