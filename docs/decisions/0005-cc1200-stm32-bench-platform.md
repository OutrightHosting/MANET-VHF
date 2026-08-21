# ADR-0005: CC1200 + STM32F4 bench platform; SDR and adapted DMR rejected

**Status:** Accepted
**Date:** 2026-08-21 (recorded; decision predates the repository)
**Phase:** Pre-development

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
