# Decision log

Architecture decisions for the narrowband voice MANET. One file per decision, numbered in the
order taken, never renumbered.

Most of ADR-0001 through ADR-0005 record decisions that were already made before this repository
existed — they are documented in [the engineering brief](../engineering-brief.md) §4 and §5 as
"decided". Writing them up here is not re-opening them. It is recording *why*, so that when Phase 0
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
| [0005](0005-cc1200-stm32-bench-platform.md) | CC1200 + STM32F4 bench platform; SDR and adapted DMR rejected | Accepted |
| [0006](0006-c-core-python-harness.md) | One protocol implementation in C, driven by a Python simulation harness | Accepted |

## Format

```
# ADR-NNNN: Title

Status:  Proposed | Accepted | Superseded by ADR-NNNN
Date:    YYYY-MM-DD
Phase:   Pre-development | Phase 0 | Phase 1 | ...

## Context
## Decision
## Consequences
## Reversal trigger
## Alternatives rejected
```
