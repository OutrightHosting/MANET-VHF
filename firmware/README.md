# `firmware/` — STM32F4 target (Phase 1)

Empty until Phase 0 passes. **Do not buy hardware until it does.**

Platform layer for the bench build: CC1200 driver, GPS/PPS discipline, audio path, Codec2 3200
integration, and the main loop that drives [`core/`](../core).

Contains no protocol logic. The MAC and routing state machine is the one in `core/`, compiled for
`arm-none-eabi` — see [ADR-0006](../docs/decisions/0006-c-core-python-harness.md).

## First measurements when hardware arrives

In this order, because later work is invalid if either fails:

1. [OQ-0001](../docs/open-questions.md#oq-0001) — achievable gross bit rate at 25 kHz 4FSK.
2. [OQ-0010](../docs/open-questions.md#oq-0010) — RX→TX turnaround, against the guard interval the
   frame structure assumes.

Target platform per [ADR-0005](../docs/decisions/0005-cc1200-stm32-bench-platform.md): 4× CC1200
evaluation modules, 4× STM32F4-class dev boards, attenuators and dummy loads. Nothing radiates —
no licence required for bench work.
