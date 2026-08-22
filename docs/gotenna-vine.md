# VINE — the control plane we are already 90% carrying

Read of [VINE: Zero-Control-Packet Routing for Ultra-Low-Capacity Mobile Ad Hoc
Networks](https://ieeexplore.ieee.org/document/9020768) (Dusia, R. Ramanathan, W. Ramanathan,
Servaes & Sethi — goTenna Inc. and University of Delaware, **MILCOM 2019**), plus goTenna's
[Aspen Grove](https://gotenna.com/pages/aspen-grove) protocol suite.

**Why this one is worth more than most:** they are in **under 25 kHz** — our channel — they
are shipping it on hardware, and in October 2023 they
[demonstrated real-time voice on it](https://gotenna.com/blogs/newsroom/gotenna-demonstrates-significant-milestone-for-narrowband-mesh-radio-voice-capability-lockheed-martin-ventures-invests-in-gotenna),
funded by US Customs and Border Protection. Their reference [1] is NBWF. They are solving our
problem, in our bandwidth, and they got to a product.

---

## 1. We already carry almost every field VINE needs

VINE builds routing state purely by **inspecting the headers of data packets**. No Hellos, no
route requests, no link-state updates. Three fields do the work — the sender, the previous
sender, and a hop count — and from each received packet a node creates a 1-hop gradient
toward the sender, a 2-hop gradient toward the previous sender, and a *k*-hop gradient toward
the source where *k* is the packet's cost-from-source.

Their own accounting of what that costs (§III-C) is the striking part: every field except
`prevSender` and `costFromSource` already exists somewhere in any MANET stack, and
`costFromSource` is derivable as `MAX_TTL − ttl`. **So the entire additional cost of the
protocol is one field.** They put it at 2 bytes, about 2%.

Against our header:

| VINE field | Ours | Status |
|---|---|---|
| `source` | `src` | ✅ have it |
| `destination` | `dst` | ✅ have it — inert ([B-09](backlog.md)) |
| `sender` | `prev` | ✅ have it — stamped by each relay |
| `seqNum` | `seq` | ✅ have it |
| `ttl` | `ttl` | ✅ have it |
| `costFromSource` | — | ✅ derivable, `MANET_VOICE_TTL − ttl` |
| `targetReceiver` | — | not needed while we broadcast |
| **`prevSender`** | — | ❌ **the one field we lack** |

**One 8-bit field.** We added `prev` for [OQ-0018](open-questions.md#oq-0018) — because the
forwarding rule had to test the sender of a copy rather than its origin — and that was exactly
VINE's `sender`. We stopped one field short of a routing protocol without knowing it.

Cost: 8 bits of the 94 currently spent on FEC, taking it from 16% to 14.6%.

## 2. Our barrage flood is already VINE's fallback mode

The obvious objection to inferring routes from data is: what happens when there is no data?
VINE's answer (§III-C) is that when no gradient exists or it has expired, the packet is
**broadcast** — and that this is self-balancing, because no gradient implies no traffic, so
flooding is affordable exactly when it is needed.

That is what [ADR-0011](decisions/0011-barrage-relaying.md) already does. **Barrage is the
flood; VINE's gradients are the narrowing.** The two compose rather than compete:

- **No gradient** → barrage flood. Today's behaviour, unchanged.
- **Gradient present** → forward along it. This is the corridor that
  [W-04](backlog.md) (Controlled Barrage Regions) was going to need hop counts for — and
  VINE's gradients *are* those hop counts, obtained free.

So the single highest-value unbuilt item on our backlog has a published, measured, shipped
mechanism sitting under it, requiring one 8-bit header field.

## 3. We already independently built their reliability mechanism

VINE provides per-hop reliability by **overhearing the next hop forward the packet** and
treating that as an implicit acknowledgement, retransmitting if it does not appear.

That is `manet_sched_heard` and `manet_sched_suppress` — our passive acknowledgement, arrived
at independently. Worth noting their stated caveat: implicit acknowledgement fails with
directional antennas, and they accept it because omnidirectional is near-universal. Ours are
helicals, so the caveat does not bite.

## 4. The traffic threshold, and why voice sits far above it

Gradients need traffic to stay fresh, so there is a rate below which flooding is simply better.
They put the crossover very low: in a 6×6 grid with a one-minute expiry, VINE beats flooding
above roughly **4 packets per minute**.

Voice at a 160 ms frame is **375 payloads per minute**, about a hundred times their threshold.
While anyone is talking, gradients would be trivially fresh.

**The gap is silence.** Their `GradientStateExpiry` is 60 s; our beacons run every 5.28 s.
Between talkspurts there is no data to infer from. So the honest design is *piggyback while
active, beacon while idle* — which still lets the beacon rate drop a long way, and beacons
currently cost **9.0% of slots** ([OQ-0004](open-questions.md#oq-0004)).

## 5. What does not transfer

- **Their MAC.** G-CSMA, to be replaced by SPIN (Slot Pinning). We rejected CSMA for voice in
  [ADR-0002](decisions/0002-tdma-slot-pipelining.md) because it gives no bounded latency, and
  that reasoning is unaffected. Only the network layer transfers.
- **Sleep-wake cycling.** Aspen Grove's power strategy. A radio that sleeps is not relaying,
  and ours must always relay.
- **Their traffic model.** VINE was evaluated on 50-byte packets every 10–30 s. The MILCOM
  results are a *messaging* protocol's results. Their 2023 voice demonstration says the suite
  extends to real-time voice, but that announcement carries no figures at all — no bandwidth,
  no vocoder, no hop count, no latency. Treat the voice claim as existence proof, not data.

## 6. Two things this settles elsewhere

- **[OQ-0029](open-questions.md#oq-0029) — did anyone make automatic relaying work in 25 kHz?**
  FFI deferred it in 2011 as possibly infeasible. goTenna ship it, in under 25 kHz, on
  handheld hardware, with Lockheed Martin Ventures invested and a US federal customer. That is
  the strongest counter-evidence available to the concern that our premise is unachievable.
- **[H-04](backlog.md) — what happened to NBWF?** VINE's reference [1] is *NATO STANAG
  5631/AComP-5631, Narrowband Waveform Physical Layer, Ratification Draft, Edition 1, 2015*,
  and its reference [21] is a MILCOM 2015 paper on networking for next-generation NBWF radios.
  **NBWF reached a ratification-draft STANAG**, so it did not die with the 2011 report. Whether
  automatic relaying was ever added to it remains open.
