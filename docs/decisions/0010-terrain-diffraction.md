# ADR-0010: Terrain is modelled per-link by knife-edge diffraction, not as an environment

**Status:** Accepted
**Date:** 2026-08-21
**Phase:** Phase 0
**Closes:** [OQ-0019](../open-questions.md#oq-0019)

## Context

Every propagation figure in Phase 0 was produced by applying one environment — an exponent
and a vegetation term — uniformly to every link. Under that model *blocked* and *distant*
are the same thing, and the brief describes a situation where they are emphatically not:

> "Front of group cannot hear back of group… A ridge or 200 m of woodland kills a link that
> would work over open ground."

Correcting the vegetation model ([OQ-0023](../open-questions.md#oq-0023)) made this
impossible to defer. Single-hop woodland range rose from 528 m to 4.42 km, at which point a
realistic dispersed group is entirely within direct range of itself and **relaying never
engages**. The uniform model had been manufacturing the need for a mesh out of excessive
foliage attenuation — not out of the mechanism the brief actually describes.

Vegetation cannot produce the required effect even in principle: ITU-R P.833-10 equation (1)
saturates at about 11 dB. A ridge does it with 23 dB and some distance.

## Decision

A height field, and **ITU-R P.526 single knife-edge diffraction sampled along the ground
profile between each specific pair of radios** — `sim/manet/terrain.py`, applied in
`Channel.rx_dbm_between`. Obstruction is a property of a *pair*, not of the world.

The default terrain is `Flat`, for which the diffraction term is skipped entirely and the
model reduces exactly to the previous uniform one. Existing results are not silently altered;
terrain is opt-in per scenario.

## Consequences

- **The product is now expressible as geometry.** A hill 80 m high with groups either side
  2.4 km apart: valley to valley −131 dBm, blocked; either valley to the hilltop −106 dBm,
  heard. Two groups that cannot hear each other and anyone on high ground between them who
  can hear both — a repeater on a hilltop that nobody sited, nobody licensed, and that
  happens to be whoever is standing up there.

- **The control is the part that matters** and it reproduces: remove the hilltop group and
  the network severs completely — 4 of 8 reachable, zero relays, nothing delivered.
  `sim/scenarios/hill.py`.

- **It exposed how much the reach figures were worth.** Protocol conclusions — convergence,
  suppression, election, reuse distance — are about topology and hold whatever creates it.
  Reach and delivery figures are not, and every one predating this ADR inherits the uniform
  model's errors.

- **It gave the project its hardest case, and nothing gates on it.** Far-group delivery
  through the hill is 72.3%. The Phase 0 gate tests cluster, latency, mobility, partition
  and beacons — not this. That is a gap in the gate, not in the model.

- Single knife-edge is the simplest defensible choice. Multiple obstructions, rounded
  hills and ground reflection are all unmodelled; Deygout or Epstein-Peterson would be the
  next step if a scenario needs two ridges.

## Reversal trigger

Phase 2 field measurement shows diffraction loss over real terrain differing enough from
single knife-edge to change which links close — most likely where a "ridge" is really two
obstructions and the single-edge approximation under-predicts loss.

## Alternatives rejected

- **Per-link attenuation table.** Would express blocking, but as an input rather than a
  consequence of geometry — so it could not answer *where should someone stand*, which is
  the question the product exists to answer.
- **Full terrain database (SRTM) with a real propagation engine.** Correct, and far beyond
  what Phase 0 needs. The point is to test protocol behaviour against a topology that
  blocks some pairs and not others; a synthetic ridge does that and runs in-process with no
  dependencies, which [ADR-0006](0006-c-core-python-harness.md) requires.
- **Keep the uniform model and shrink the range.** Considered and rejected as dishonest:
  it reproduces the *symptom* (links fail, relaying engages) by breaking the one thing the
  vegetation correction had just got right.
