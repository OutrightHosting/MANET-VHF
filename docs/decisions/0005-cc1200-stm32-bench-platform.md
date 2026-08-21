# ADR-0005: CC1200 + STM32F4 bench platform; SDR and adapted DMR rejected

**Status:** **Re-opened 2026-08-21** — the bill of materials below cannot be bought
**Date:** 2026-08-21 (recorded; decision predates the repository)
**Phase:** Pre-development

> ## Re-opened: there is no CC1200 evaluation module in our band
>
> The decision below specifies *"Four TI CC1200 evaluation modules"*. Checked against TI's
> own tool pages while confirming the CC1200 stays as the modem:
>
> | Part | Band | Status |
> |---|---|---|
> | [CC1200EMK-420-470](https://www.ti.com/tool/CC1200EMK-420-470) | 420–470 MHz | Buyable, wrong band |
> | [CC1200EMK-868-930](https://www.ti.com/tool/CC1200EMK-868-930) | 868–930 MHz | Buyable, wrong band |
> | CC1200EM_169 rev 1.2 | 169 MHz | **Reference design only** — schematic, layout, BOM. Not a product |
> | [CC1120EMK-169](https://www.ti.com/tool/CC1120EMK-169) | 169 MHz | **Buyable now.** 2 modules per kit, in stock, not sold in US/Canada |
>
> The CC1200 silicon covers 164–190 MHz. The *board* does not exist. So Phase 1 has to
> choose a route, and the choice is worth making deliberately because it also touches which
> part ships.
>
> **What is common to both routes.** [CC1120DK and CC1200DK are both built on the same
> SmartRF TrxEB motherboard](https://www.ti.com/tool/CC1200DK), and the TrxEB is sold
> separately as [SMARTRFTRXEBK](https://www.ti.com/tool/SMARTRFTRXEBK). EM daughtercards plug
> into it either way. **The motherboard purchase is chip-independent and can be placed
> immediately**, which matters because it is the long-lead item.
>
> Note the TrxEB carries an MSP430F5438A, not the STM32F4 this ADR pairs with. That is not a
> conflict, it is two rigs for two jobs: TrxEB + SmartRF Studio for RF characterisation
> ([OQ-0001](../open-questions.md#oq-0001), [OQ-0024](../open-questions.md#oq-0024)), and EM
> wired to an STM32F4 over SPI for protocol and timing work
> ([OQ-0010](../open-questions.md#oq-0010)), where 16 KB of RAM at 25 MHz would not carry
> Codec2 and the core.
>
> ### Route A — fabricate CC1200EM_169 rev 1.2
> Keeps the part. TI has already done the VHF matching network and layout, so this is a PCB
> spin against a proven design rather than an RF design exercise. Costs weeks and a fab run.
>
> ### Route B — CC1120EMK-169, and reconsider which part ships
> Buyable today, and 169 MHz sits inside the preferred High Band
> ([OQ-0027](../open-questions.md#oq-0027)). It is a different chip, so this is only sensible
> if the CC1120 is a candidate to ship. **On the evidence it should be**, per
> [TI's own comparison](https://e2e.ti.com/support/wireless-connectivity/other-wireless-group/other-wireless/f/other-wireless-technologies-forum/304580/what-are-the-main-differences-between-the-cc1200-and-the-cc1120):
>
> | | CC1120 | CC1200 |
> |---|---|---|
> | Optimised for | **Narrowband** | Wideband; covers narrowband to 12.5 kHz |
> | VHF band | 164–192 MHz | 164–190 MHz |
> | 4-GFSK | Yes | Yes |
> | Output power | +16 dBm | +16 dBm |
> | Sensitivity | *Equal* at the same rate, deviation and RX bandwidth | *Equal* |
> | Adjacent channel selectivity | **64 dB at 12.5 kHz offset** | "slightly lower blocking performance" |
> | AES | No | Yes |
> | VHF board | **In stock** | Reference design only |
>
> **The blocking figure is the one that should decide it, and the reason is specific to this
> project rather than to TI's comparison.** In a point-to-point link, near-far is an edge
> case. In a barrage-relay mesh it is the *normal operating condition*: a relay 50 m away and
> an originator 4 km away transmit into the same receiver in adjacent slots, and
> [OQ-0019](../open-questions.md#oq-0019) already records that terrain makes this worse — a
> blocked wanted signal is weak over a short distance while an unobstructed interferer is
> strong over a long one. Receiver blocking performance is a first-order concern here and it
> cannot be recovered in software.
>
> The AES column is the only real argument the other way, and it is weak:
> [OQ-0007](../open-questions.md#oq-0007) wants encryption, but the STM32F4 does AES at voice
> rates without noticing, and [ADR-0006](0006-c-core-python-harness.md) already forbids the
> transceiver from owning protocol logic.
>
> **Recommendation: Route B.** Order 4× SMARTRFTRXEBK and 2× CC1120EMK-169, and re-write this
> ADR around the CC1120. Route A stays available and cheap to fall back to — the two parts are
> pin-similar enough that the STM32-side driver is the same shape, which is exactly the
> transceiver abstraction ADR-0006 already requires.
>
> **Not decided.** This changes which part ships and needs sign-off.

## Context

Phase 1 needs hardware on which slot timing can actually be validated — 15 ms slots with guard
intervals measured in hundreds of microseconds. The instrument has to be capable of the discipline
being tested, and it has to expose the layer being developed.

## Decision

Four TI CC1200 evaluation modules paired with four STM32F4-class dev boards, wired through
attenuators and dummy loads. Plus a TinySA Ultra or equivalent for basic spectrum measurement.

CML Microcircuits CMX7164 is the likely better *production* part — a multi-mode narrowband modem
built for custom protocols rather than fixed standards. Engage CML early to establish exactly what
timing control it exposes, but do not block Phase 1 on it.

## Consequences

- Total cost is a few hundred pounds and no licence is required — nothing radiates.
- The STM32F4 runs both the protocol core and Codec2, so Phase 1 exercises the real code path.
- Achievable gross bit rate at 25 kHz on the CC1200 is unverified. The whole slot structure rests on
  it. This is [OQ-0001](../open-questions.md), and it is the first thing to measure when hardware
  arrives.
- Two transceiver families in play (CC1200 for bench, CMX7164 possibly for production) means the
  protocol core must not bake in transceiver-specific timing. It talks to a radio abstraction; the
  driver is what changes. This is enforced by [ADR-0006](0006-c-core-python-harness.md).

## Reversal trigger

CC1200 cannot reach a gross rate at 25 kHz sufficient for any workable slot/vocoder combination, or
its RX→TX turnaround will not fit a compliant guard interval.

## Alternatives rejected

- **SDR — ADALM-Pluto, LimeSDR, HackRF.** USB round-trip latency is milliseconds against a 15 ms
  slot budget; slot discipline cannot be validated on it. Also simply the wrong instrument: a
  20 MHz-capable radio developing a 25 kHz waveform.
- **Adapted DMR handhelds — MD-UV380, GD-77.** The HR-C6000 baseband owns the framing and is locked
  to DMR's two-slot structure. The exact layer this project must replace is the one that is closed.
- **Motorola DP4800 and similar.** Signed, closed firmware. No baseband access at all.
