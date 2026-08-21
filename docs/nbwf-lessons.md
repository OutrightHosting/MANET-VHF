# What FFI already learned, and we had not recorded

A full read of [FFI-rapport 2009/01894](reference-nbwf-ffi-2009-01894.pdf), *NATO Narrowband
Waveform (NBWF) — overview of link layer design* (Jodalen, Solberg & Haavik, 28 March 2011).

The document was previously mined for one number — the acquisition preamble, in
[preamble-budget.md](preamble-budget.md). That was a small part of what is in it. This is the
rest, ordered by how much it should change what we do.

Their system is not ours: 50 W vehicular, VHF low band, MELPe vocoders, dedicated relays.
But it is the same problem — **voice over a multi-hop ad-hoc network in a 25 kHz channel** —
attacked by a NATO-funded team over four years with a discrete-event simulator. Where they
concluded something, it is worth more than our opinion.

---

## 1. They tried to build our design and backed off

§4.4 is the most important paragraph in the document for this project. On automatic relaying
of voice without dedicated relay nodes — which is exactly our architecture, every handset a
relay — they write that they are **"not sure if such a protocol is feasible within 25 kHz
bandwidth"**, that extensive simulation is needed to answer it, and that in order to produce a
draft specification sooner they instead propose dedicated relays.

Their fallback keeps two properties: relaying is TDM rather than FDM, so one radio and one
frequency; and any node *can* act as a relay — **if it is configured to do so**. Configured,
not discovered. They note that automatic relaying should be investigated later "if possible".

**This is not proof that our design is impossible.** They were solving for 250 ms delay,
250-node subnets, four vocoders, simultaneous IP data, and NATO-wide interoperability. We
have one vocoder, twelve to fifty users, no data requirement, and 500 ms. Removing four of
their five constraints is exactly the kind of thing that turns infeasible into feasible.

**But it belongs in the risk register, and it was not there.** The one team that has publicly
tried this looked at automatic relaying in 25 kHz and deferred it. Recorded as
[OQ-0029](open-questions.md#oq-0029).

## 2. They optimise the opposite thing to us, and they may be right

§4.1. Their N1 mode (20 kbps) reaches three to four times as far as N4 (96 kbps). Their
conclusion for multicast: use the **lowest** data rate, to reach as many nodes as possible in
one hop. At N1 they state only 1–2 relays are needed to cover more than 50 km.

We have spent this project maximising **hop count** — four hops, then seven, chasing twelve.
FFI maximise **range per hop** and then need almost no hops at all.

Every hop we add costs latency, airtime, duty cycle ([OQ-0026](open-questions.md#oq-0026)),
and one more radio that has to be in the right place. A design that needs seven hops is more
fragile than one that needs two, at equal coverage. **The budget may be better spent on link
margin — lower vocoder rate, more FEC, better sensitivity — than on hop count.**

This is a strategy question we have never actually asked, and it cuts against the direction
of the last several days' work. [OQ-0030](open-questions.md#oq-0030).

## 3. Their PHY beats ours by 56%, using the modulation our own review said we needed

Table 2.1, simulated block error rates:

| Mode | User data rate | Channel symbol rate | SNR in 25 kHz @ BLER 0.1 |
|---|---|---|---|
| NR | 10 kbit/s | 30 ksym/s | 0.1 dB |
| **N1** | **20 kbit/s** | **30 ksym/s** | **2.5 dB** |
| N2 | 31.5 kbit/s | 42 ksym/s | 9.2 dB |
| N3 | 64 kbit/s | 80 ksym/s | 18.2 dB |
| N4 | 96 kbit/s | 128 ksym/s | 24.6 dB |

Modulation is **CPM concatenated with a convolutional encoder**, 12 ms interleaver.

N1 delivers **20 kbit/s of user data** — after coding — in 25 kHz, at 2.5 dB SNR. Our
assumption is 19.2 kbps *gross* with 16% FEC, so roughly 16 kbps of user data. **They get
25% more payload in the same channel, and they get it at 30 ksym/s where our 4FSK runs at
9600.**

Two consequences, and the second is larger than the first:

- **[OQ-0001](open-questions.md#oq-0001) has a published answer to aim at.** 19.2 kbps gross
  was scaled from DMR. A CPM design achieves 30 ksym/s in the same channel. Our figure is
  conservative, and the bench should be measuring against 30 ksym/s rather than confirming
  19.2 kbps.
- **CPM is exactly what [literature-review.md §119](literature-review.md) said barrage
  relaying requires.** That section's objection to [ADR-0011](decisions/0011-barrage-relaying.md)
  is that BRN combining needs CPM plus phase dithering and MLSE, and 4FSK has none of it. FFI
  chose CPM independently, for throughput. **The modulation that fixes our throughput problem
  is the same one that would underwrite [OQ-0028](open-questions.md#oq-0028).**

Worth noting the CC1200 and CC1120 both offer MSK and GFSK, which *are* continuous-phase
formats. Whether that is enough to get the combining behaviour, or whether it needs the full
CPM-plus-interleaver treatment, is a bench question — but it moves OQ-0028 from "hope the
capture effect saves us" to "there is a known modulation family that makes this work".

## 4. Their propagation model is far more pessimistic than ours

§2.3. FFI note that the NBWF physical-layer draft's own propagation model gives **"highly
exaggerated values"** for existing tactical VHF radios, and discard it in favour of the Egli
model with a path-loss exponent of **γ = 4**.

**Our woodland exponent is 2.97** ([OQ-0023](open-questions.md#oq-0023)), tuned to make a
4.8 km handheld range come out. FFI, modelling the same band, use 4 — and explicitly warn
that the model producing longer ranges was the one they threw away.

Also from the same section, and directly useful:

- A legacy CNR radio's sensitivity is given as **−116 dBm** for 16 kbit/s digital voice at
  BER 0.1. That is exactly the figure in our `LinkBudget`, arrived at independently. Good.
- Their range figures are **distributions, not points**: mode N1 gives 22.0 km median, 13.1 km
  at the 90th percentile and 36.9 km at the 10th — roughly a 1:3 ratio between quantiles. We
  quote single numbers for reach. We should quote a median and a 90%.

This is a genuine challenge to every range figure in the project and it is not resolved by
argument — [OQ-0023](open-questions.md#oq-0023) should be re-opened against γ = 4.

## 5. Answers to open questions we already had

**[OQ-0004](open-questions.md#oq-0004) — where control traffic lives.** §4.8 gives a slot
taxonomy we do not have. Their frame is nine slots of 22.5 ms:

| Category | Name | Usage |
|---|---|---|
| Fixed | MV | Multicast voice signalling or transfer only |
| Fixed | SF | Superframe allocation — guarantees each node a minimum capacity |
| Dynamic | DU | Dual use: data, **pre-empted** by voice |
| Dynamic | GU | General use: data or selective call, **never** pre-empted by voice |

At least one MV slot always exists so a call can be set up instantly, and they recommend
**one additional MV slot per relay**. The GU category exists specifically so that a private
call cannot be pre-empted by a group call — a distinction we have not made.

**[OQ-0009](open-questions.md#oq-0009) — voice channel access.** §4.2 and §4.6.2: PTT
triggers an RTS from the sender and a number of CTS replies from selected one-hop neighbours,
sent **sequentially**, and the neighbours chosen are those that must receive it *or that are
selected as relays*. The exchange also makes two-hop neighbours aware of the talk spurt so
they stay off those slots — hidden-terminal protection by explicit signalling, where we use
NAMA's two-hop election. They note the 250 ms setup requirement may be hard to achieve in
some topologies.

**[OQ-0021](open-questions.md#oq-0021) — private calls.** §4.7 is sobering. Their selective
call is confined to GU slots, and with one configured relay only three GU slots exist, with
two relays only one. Their conclusion is that a **one-hop** selective call is all the network
offers. With two relays a private call is not possible at all. Our `dst` field is inert; NATO
shipped a design where the equivalent feature barely exists.

**[OQ-0022](open-questions.md#oq-0022) — latency budget.** §4.2 confirms the requirement is
either 250 ms or 500 ms depending on which paragraph of the requirements document applies.
Our choice of 500 ms is one of their two, not an invention.

**[OQ-0003](open-questions.md#oq-0003) — synchronisation.** §4.11: nodes must be synchronised
to within **1.5 ms**, GNSS is used when available, and if it is not, a sync exchange protocol
is needed which **is not yet defined**. Two things follow: our GPS-disciplined approach is
1500× tighter than their requirement, which is the basis of the preamble argument in
[OQ-0024](open-questions.md#oq-0024); and network-derived sync was left unsolved by a team
that had made GNSS-independence a hard requirement.

## 6. Budget items we have not costed

- **Crypto IV and link PCI: ~120 bits (15 bytes) per burst**, stated twice (§4.3, §4.5). Our
  header is 42 bits with no encryption. [OQ-0007](open-questions.md#oq-0007) wants AES; an
  IV is not free, and 120 bits against our 704-bit slot would take FEC from 16% to roughly
  4%. This is a real cost that has never appeared in `make budget`.
- **Voice may not fit in one slot.** At 20 kbps their MELPe 2400 needs **two** slots per
  frame, so a relay hop costs 2 × 22.5 ms, not one slot. Our Codec2 3200 does fit one 40 ms
  slot — but only just, and only at the current FEC. If FEC rises, this is the first thing
  that breaks, and it doubles per-hop latency when it does.
- **Relaying is expensive in capacity, not just latency.** Their Table 4.1: multicast voice
  with two relays consumes six of nine slots, leaving three for everything else. Several
  vocoder/relay combinations are simply marked *not possible*.

## 7. A requirement we have never considered

§4.12 lists **radio silence** as something that should be a requirement and is not one.

In a network where every handset relays, a node in radio silence stops relaying — so the
feature is not a user preference, it is a topology change. Nothing in our design contemplates
it, and for the actual users here (youth organisation leaders, some of whom will be indoors,
in a meeting, or simply want the radio quiet) it is more plausible than it is for a fire team.

## 8. The caveat on all of the above

§5, dated March 2011: FFI had built a discrete-event simulator to validate the link design
and state that **it had not yet been used to any extent**. The design in this document is
therefore *reasoned*, not *measured* — the same status as most of our own decisions, and it
should not be treated as a validated reference. Their 8 ms acquisition figure carries its own
footnote saying it is an estimate, which is what
[preamble-budget.md](preamble-budget.md) already established.

The prototype software they call for was to be built by industry; FFI state they lack the
resources. Whether NBWF was ever fielded is not answered by this document and is worth
finding out — if it was not, that is informative too.
