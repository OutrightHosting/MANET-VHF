# ADR-0001: Narrowband VHF on licensed UK Business Radio spectrum

**Status:** Accepted
**Date:** 2026-08-21 (recorded; decision predates the repository)
**Phase:** Pre-development

## Context

The operational problem is terrain, not distance. A ridge or 200 m of woodland kills a link that
works over open ground. Any band choice that trades propagation for bandwidth makes the actual
problem worse, however much easier it makes the engineering.

Every existing product sits on one side or the other of a gap:

- Commercial MANETs (Silvus, Persistent, DTC, TrellisWare) mesh properly, but at 1.2–5 GHz, with
  worn node form factors, external audio, and ~£10k/node. Wrong propagation, wrong ergonomics,
  wrong price.
- Narrowband PMR standards (DMR, TETRA, dPMR, NXDN) have the right propagation and the right form
  factor, but no multi-hop — every one of them assumes a fixed repeater we cannot license where
  the group operates.

Licence-exempt bands were considered and eliminated: 868 MHz carries UK duty cycle limits of 1–10%,
which is disqualifying for traffic that spikes during a safety incident (hard requirement 4).
2.4 GHz at 100 mW is roughly 25 dB down on VHF at 5 W before terrain is even considered.

## Decision

Operate on licensed UK Business Radio VHF spectrum — Mid Band (137.9625–165.04375 MHz) or High
Band (165.04375–173.09375 MHz) — in a 25 kHz single-frequency simplex channel, 4FSK, 5 W.

25 kHz over 12.5 kHz, because the slot and vocoder budget does not close in 12.5 kHz.

## Consequences

- Propagation and terrain penetration are as good as this project can get in a hand-portable.
- No duty cycle restriction. Safety traffic is unconstrained.
- Gross bit rate is ~19.2 kbps *at best*, and every design decision downstream lives inside that.
  This is the binding constraint on the entire system — see [OQ-0001](../open-questions.md) and
  [OQ-0002](../open-questions.md).
- Requires an Ofcom licence to operate and an Innovation and Trial licence to test over the air
  (Phase 2). Bench work into dummy loads needs neither.
- Requires RED conformity for the finished product: EN 300 113-2 / EN 301 166-2, EN 301 489-1
  and -5, EN 62368-1, EN 62479 / EN 50665. Self-assessment under Module A is available.
- The antenna must be a ~15–20 cm helical. A quarter wave at 155 MHz is 48 cm and unwearable.
  Accept 3–6 dB loss as the price of hard requirement 1.

## Reversal trigger

A 25 kHz simplex assignment turns out to be unobtainable in both Mid and High Band. That is a
spectrum-availability question, not an engineering one, and it should be settled with Ofcom before
Phase 2 — not after.

## Alternatives rejected

See [engineering brief appendix](../engineering-brief.md#appendix--why-the-obvious-alternatives-were-eliminated)
for the full elimination table.
