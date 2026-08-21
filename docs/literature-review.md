<!-- Literature review, 2026-08-21. Produced by a five-lens survey of primary sources
     (IETF RFCs, MobiCom/MobiHoc/SenSys papers, 3GPP specs) checked against this design.
     Claims marked [V] were spot-checked against a primary source; [U] are second-hand.

     Read this before writing more MAC code. Several decisions in docs/decisions/ are
     confirmed by it, several are corrected by it, and one whole line of work — inventing
     slot assignment — turns out to duplicate a standard badly. -->

# MANET-VHF — Literature Review Brief

**For:** the engineer. **Date:** 2026-08-21. **Basis:** five independent literature reviews (routing, TDMA, voice, implementations, capacity) plus a read of the repo at `8238da7`. Claims below marked **[V]** were spot-checked against a primary source this session; **[U]** could not be verified and should be treated as second-hand.

**Before anything else — two of the reviews were briefed against a stale design summary.** The summary they were given says "NO topology-control messages" and "destination address is NEVER READ." The first half is already reversed in the repo: `docs/decisions/0003-olsr-mpr-routing.md` is amended by ADR-0007, and OQ-0011 is closed with "TC stays." The second half is still true *in code* — `core/src/frame.c` writes and validates `hdr.dst` (lines 22, 59, 83) and nothing in `mpr.c`, `slot.c` or `dedup.c` ever reads it. So: discount review commentary that argues against dropping TC (settled), but take seriously everything about the dead destination field (live defect).

---

## 1. What we got right

These are decided. Stop relitigating them, and cite these when someone asks.

**Slot pipelining (ADR-0002) is a named, fielded architecture, and it is latency-optimal.** It is a Barrage Relay Network (TrellisWare TSM) in the tactical literature and *pipelined flooding / concurrent transmissions* (Glossy, Ferrari et al., IPSN 2011) in the sensor-network literature. Glossy is stated to achieve the theoretical lower latency bound for one-to-all delivery of a packet in a multi-hop half-duplex network **[U — claim taken from the Zimmerling/Mottola/Santini survey, arXiv 2001.08557, not from the Glossy paper itself]**. Either way, "receive in slot *n*, transmit in slot *n+1*" is not an invention of ours and is not the suspect part of the design.

**Four slots is right, and there is a second, more robust argument for it than the one in ADR-0008.** ADR-0008 rests on a 10 dB capture threshold and a 3.2 path-loss exponent, and its own reversal note admits that below 9.1 dB the result flips. Gupta & Kumar's Protocol Model gives the same answer with no dB and no exponent at all: a transmission over distance *d* survives if every other simultaneous transmitter is at least (1+Δ)·d from the *receiver*. With N slots and one hop per slot the co-slot reuser is N hops from the transmitter, hence (N−1)·d from the receiver, so Δ = N−2. Four slots gives Δ=2, three gives Δ=1, and Li et al.'s measured 802.11 hardware figure is Δ = 550/250 − 1 = 1.2 **[V — 250 m reception / 550 m interference confirmed verbatim in the Li et al. PDF, §3.2]**. Four passes, three fails, same conclusion, no fragile constants. Put this in ADR-0008 as a backstop. *(The Δ = N−2 derivation is mine, not quoted from Gupta & Kumar.)*

**Our injection rate is compliant with the published reuse rule.** One 192-bit payload per 60 ms frame = one packet per 4 slots, so M=4 against TrellisWare's stated minimum of M≥3 **[U — patent text not verified this session]**. The pipelining is not the source of our collisions.

**Passive acknowledgement and half-duplex PTT are load-bearing capacity requirements, not tolerated limitations.** An explicit end-to-end ACK is a counter-propagating flow and halves chain capacity outright (the documented TCP-over-multihop pathology). Passive ACK dates to DARPA PRNET (Jubin & Tornow, Proc. IEEE 1987). Write this down before someone "improves" it into a redesign — see §4 for the arithmetic proof that counter-propagating flows cannot share our chain at all.

**Moving from sender-decided MPR to a receiver-decided rule was the right direction.** Williams & Camp (MobiHoc'02) found sender-decided schemes (MPR, AHBP, Dominant Pruning) degrade worst under mobility because stale 2-hop state corrupts forwarder selection, while receiver-decided schemes self-heal by relaying more. Our "frontier radio relays anyway" heuristic is AHBP-EX's mobility rule, independently re-derived, and it is a recognised technique.

**Narrowband is an unexpected advantage for this technique, and no document of ours says so.** Concurrent-transmission flooding needs sub-microsecond alignment at 802.15.4 rates (~0.5 µs) and nanoseconds at Wi-Fi rates. Our 4FSK symbol at 9600 baud is 104 µs — roughly 200× the slack — and propagation spread over 15 km is ~50 µs, so every concurrent relay lands inside the capture window by two orders of magnitude. Capture in our system is decided by power ratio alone, which is exactly the regime ADR-0008's analysis assumes. Record that as designed margin.

---

## 2. What we are reinventing badly

### 2.1 Slot assignment — the prime suspect, and the fix is nearly free

Our rule (hash own address into a bounded pool, no detection, no resolution) is the *first step* of RFC 9033, 6TiSCH Minimal Scheduling Function, Standards Track — and MSF's entire value is the three steps we skipped **[V, RFC 9033]**:

- `slotOffset(MAC) = 1 + hash(EUI64, length(Slotframe_1) − 1)`, `channelOffset(MAC) = hash(EUI64, NUM_CH_OFFSET)`
- the cell hashed from a node's **own** address is an **AutoRxCell**: `TX=0, RX=1, SHARED=0`. You *listen* there. You do not blindly transmit there.
- the **AutoTxCell** is hashed from the **destination's** EUI-64 (`TX=1, RX=0, SHARED=1`)
- collision detection is by evidence: per-cell `PDR = NumTxAck/NumTx`, `MAX_NUMTX = 256`
- remediation: every `HOUSEKEEPINGCOLLISION_PERIOD` (default 1 min), relocate any cell whose PDR is more than `RELOCATE_PDRTHRES` (default 50%) below the best cell to the same peer.

With `MANET_BEACON_INTERVAL_FRAMES = 33` and 4 slots/frame we hash 12 radios into 132 slots. Birthday probability of at least one collision ≈ 1 − e^(−66/132) = **39%**, and the collision is permanent and undetectable — the two colliding radios never hear each other and their common neighbours hear neither. That is not a degraded link, it is two nodes structurally invisible to routing, forever.

**The better primitive is election, not hashing.** NAMA/NCR (Bao & Garcia-Luna-Aceves, MobiCom 2001, Eq. 1–2) **[V — text extracted from the paper]**: for contention context *t*, compute for every contender *k* in M_i ∪ {i}

> `p_k^t = Rand(k ⊕ t) ⊕ k`

where `Rand(x)` is a PRNG seeded with *x*, and transmit iff `p_i^t > p_j^t` for all j ∈ M_i. The trailing `⊕ k` guarantees uniqueness, so ties are impossible. Every node computes the identical numbers from identical inputs, so the schedule is collision-free by construction with **zero handshake messages**. M_i for node activation is the 1-and-2-hop neighbour set — which we already maintain (`MANET_MAX_NEIGHBOURS 16`, `MANET_MAX_TWO_HOP 32`) and currently throw away at the MAC layer. This is a change to a hash input plus a max-comparison over a table we already populate.

### 2.2 Duplicate suppression — we have implemented the one scheme the literature says cannot guarantee delivery

`core/src/slot.c:175`, `manet_sched_suppress()`, cancels a queued relay on hearing **any** single duplicate. That is Ni et al.'s counter-based scheme with **C = 1** — the most aggressive setting of a family that MobiCom'99 shows cannot guarantee reachability. On a voice network the failure mode is a radio that goes silent with no error indication.

The correct rule is SBA's (Peng & Lu, 2000): on each duplicate, recompute the *uncovered neighbour set* against the **union** of all transmitters heard so far, and cancel only when that set becomes empty. Note the caveat in §2.3 before implementing it.

### 2.3 The stagger does not exist, and cannot exist inside a frame

The design summary claims "candidate relays staggered by link quality." There is no stagger in the C core: `manet_sched_relay()` places unconditionally at `rx_slot + 1`. Every candidate relay at hop *n* lands in the same slot. And it could not be otherwise — SBA's Random Assessment Delay assumes a node can *hear* other relays during its delay, which slot pipelining plus half-duplex makes impossible twice over (all hop-*n* relays fire in slot *n*, and a transmitting radio is deaf).

If you want ordered relay selection that survives this MAC, the model is **LENWB**, not SBA: a node *computes* what its higher-priority neighbours will cover and suppresses deterministically, without needing to hear them. Compute-don't-listen. (Caveat: LENWB's own authors reported the base protocol performing poorly; borrow the mechanism, not the protocol.)

### 2.4 Our relay rule is the weakest member of the family it belongs to

"Relay if I reach at least one neighbour the sender does not" is verbatim **Flooding with Self Pruning** (Lim & Kim, 2001), catalogued as such by Williams & Camp §3.4. It prunes against a single sender's coverage, where SBA prunes against the union of all heard transmissions — it is the most redundant neighbour-knowledge scheme, and Williams & Camp explicitly ranked it bottom of the receiver-decided branch. We picked the right branch and then the weakest member of it.

The MAC-appropriate alternative is **E-CDS** (RFC 6621 Appendix A, Experimental) **[V]**: routers self-elect into a connected dominating set from 2-hop topology, and — the property that matters to us — "*its packet-forwarding rules are not dependent upon previous hop.*" That means no per-packet sender information is needed, which we cannot afford to carry in 192 bits, and it produces a stable ~1-relay-per-neighbourhood spine, which is the only thing a 4-slot pipeline can actually schedule.

### 2.5 Our beacon cannot carry a neighbour list, and NHDP already solved that

RFC 6130 (NHDP, Standards Track) separates `HELLO_INTERVAL` from `REFRESH_INTERVAL` and requires only that "*within every time interval equal to the corresponding REFRESH_INTERVAL, sent HELLO messages MUST collectively include all of the relevant information in the corresponding Link Set*" **[V]**. Advertise a rotating subset per beacon; guarantee coverage over the longer window; size hold times off `REFRESH_INTERVAL`. This converts our pruning input from "the sender's neighbour list" (unbounded staleness) to "…as of at most REFRESH_INTERVAL ago" (a bound you can reason about). Also adopt NHDP's rule that only symmetric links enter the 2-Hop Set **[V]** — asymmetric-link pruning is a known correctness trap.

---

## 3. What we have simply not built

Ranked by consequence.

1. **Any feedback loop at all.** Nothing in MANET-VHF can notice a slot collision, a stale reservation, or a neighbour that rebooted. Every comparable system has one: MSF's PDR + RELOCATE, USAP's conflict detection and resolution, AIS's randomised slot time-out (TMO_MIN 3 – TMO_MAX 7 frames, so every reservation expires whether or not anyone noticed) **[U]**, 6P's lollipop SeqNum for reboot detection. This is the single largest structural gap.
2. **Two-hop state at the MAC.** We collect it and discard it. Every correct distributed slot assignment since USAP (MILCOM'96) makes the constraint set the union of *my* slots, *my neighbours'* slots, and *my neighbours' neighbours'* transmit slots. Hidden two-hop collision at a shared relay is the failure that kills a relaying mesh and is invisible to any amount of listening on a half-duplex radio. (AIS tolerates exactly this failure — it never relays, so a hidden collision harms nobody. We relay. Do not copy AIS's one-hop world model.)
3. **A bounded TTL.** `MANET_TTL_BITS 5` = TTL 31. At 15 ms/hop that is 465 ms of network occupancy while the talker emits a new frame every 60 ms, and the pipeline wraps into the originator's own next transmission after four hops. Our effective diameter is 4. Meshtastic caps at 7 and ships 3 **[U]**; AODV bounds discovery with expanding ring search.
4. **A composable link metric.** Our link codes are three enum values (asymmetric / symmetric / selected-as-relay). That is not a metric and cannot be summed along a path. ETX is computable today from beacon loss with zero new messages; OLSRv2 (RFC 7181) makes metrics directional and assessed at the receiver, which is the right model for VHF and natural for a receiver-decided design.
5. **Link hysteresis.** RFC 3626 §14 (EWMA + HYST_THRESHOLD_HIGH/LOW) and RFC 6130's HYST_ACCEPT/HYST_REJECT + INITIAL_PENDING **[V]**. Without it our link codes flap on a fading channel, which changes the pruning input at every neighbour, which changes the relay set mid-talkspurt — audible dropouts correlated with someone walking behind a building. Pair with RFC 7466's `N2_lost` flag so a transient dip marks 2-hop state dormant instead of destroying it.
6. **Any channel-access protocol whatsoever.** No listen-before-talk, no backoff, no reservation, no defined behaviour for simultaneous PTT. Z-MAC's owner-priority (owner backs off T_o, non-owner backs off into [T_o, T_no]) makes a reserved control slot cost nothing when idle — the answer to OQ-0004's "a dedicated slot costs 25% of a 4-slot frame." USAP's standby-slot pattern with rotating correspondence is the same idea from the military side.
7. **Integrity protection.** Beacons carry the neighbour lists that every relay decision depends on and are unauthenticated on a channel anyone with an SDR can transmit on. Forging a neighbour list either makes you the sole relay or convinces every real relay it covers nothing new — and the second attack is indistinguishable from bad propagation. RFC 7183 (HMAC-SHA-256, mandatory-to-implement for NHDP/OLSRv2) is the specified countermeasure.
8. **A stated invariant for the relay rule.** RFC 7181 §18.3 specifies MPR validity as *properties*, not an algorithm — that is the licence to keep a bespoke rule while still being able to say what it guarantees. Right now our rule has no stated invariant, which is why we cannot tell whether it is correct.
9. **Net entry.** Radios hash and transmit immediately. The standard pattern (AIS §3.3.5, USAP) is: listen for a full assignment cycle → claim one slot by random access → pre-announce subsequent slots → settle.

---

## 4. What the literature says about our measured limits

**"A chain carries one conversation" is the known result. Our reasoning for it is wrong, and wrong optimistically.**

Li et al., *Capacity of Ad Hoc Wireless Networks*, MobiCom 2001, derive it in three steps **[V — read from the primary PDF]**: (a) nodes 1 and 2 cannot both transmit (half-duplex); (b) nodes 1 and 3 cannot both transmit, because node 2 cannot hear 1 while 3 sends — "*This leads to a channel utilization of 1/3*"; (c) interference range exceeds communication range (550 m vs 250 m), giving "*the maximum utilization of a chain of 8 nodes in the ns simulator to be 1/4*." Measured 802.11 achieves 1/7, because node 1 injects at 0.48 Mbps while node 4 can only forward at 0.26 and the surplus is carried partway then dropped.

Our "2N slots per relay (N to transmit, N to keep ears free)" is only step (a). Taken literally it predicts *two* conversations fit in four slots; ADR-0008 says one. Two of our documents disagree by 2×, and ADR-0008 is the correct one. **Replace the 2N model with the reuse factor of 4, then claim the win:** a fixed 1-in-4 TDMA schedule delivers exactly 1/4 by construction — the theoretical optimum for a single-channel half-duplex chain, and 1.75× better than the canonical CSMA measurement, because we cannot starve, cannot waste backoff, and have no hidden-terminal RTS corruption. That is the strongest property our MAC has and it is currently mis-explained in our own docs.

**It is worse than "one per chain," though: it is one per *network*.** Because we flood, a continuously-pipelined call occupies all four slots in every neighbourhood simultaneously (predecessor in k−1, self in k, successor in k+1, co-slot reuser in k+2). LWB (SenSys 2012) documents exactly this: map everything onto network-wide floods and the multi-hop topology collapses into a shared bus whose capacity divides by the number of sources **[U]**. ADR-0008's "concurrent calls when clustered: four, matching DMR" is true only in the degenerate single-hop case and collapses to one at the first relay. It also predicts OQ-0013 precisely: with all four slots busy everywhere during a call there is no residual airtime for beacons, so beacons must pre-empt voice — 9% beacon airtime is roughly one voice frame in eleven.

**On the claim that phase assignment doubles capacity — it is real but narrower than one reviewer stated.** *(Derivation mine; check it.)* A relay at hop *k* in a flow of injection phase *p* transmits in slot (p+k) mod 4 and receives in (p+k−1) mod 4. For two flows through the same node, let d = (q+k_q) − (p+k_p) mod 4 — the difference in **effective phase at that node**, not injection phase. Conflict when one flow's transmit slot equals the other's receive slot, i.e. d ≡ ±1; and d ≡ 0 is also fatal, since the node would have to transmit two different PDUs in one slot. So the only safe value is **d ≡ 2**. For co-directional flows k_q − k_p is constant along the chain, so d is constant and two conversations coexist — but only if the assignment accounts for hop offset, which the source does not know without a reservation layer. For **counter-propagating flows d changes by −2 per hop**, so it alternates between d₀ and d₀+2: if d₀ ∈ {1,3} every node conflicts, and if d₀ ∈ {0,2} the d=0 nodes conflict. **Counter-propagating flows on a shared chain always fail somewhere.** That is the arithmetic reason half-duplex PTT is a requirement rather than a limitation, and it should go in an ADR.

**What real systems do about the limit.** TETRA's single-carrier type 1A DM-REP supports exactly one call; two requires a duplex-spaced carrier pair **[V in part — the search result confirms type 1A operates on a single carrier and retransmits received bursts in a different timeslot; the "three-timeslot lag" figure is [U], ETSI returned 403]**. DMR direct mode is one call, two with DCDM across both slots in 12.5 kHz **[U]**. The whole LMR industry declined to build cascadable multi-hop direct-mode voice — P25's answer to range is one vehicular repeater hop into fixed infrastructure. NATO's NBWF, the closest published analogue (25 kHz VHF TDMA MANET, ~20 kbps, no master nodes), spent its bit budget on a 2.4 kbps vocoder to buy a 9-slot pool and then specified *configured, dedicated* relays, writing that they were unsure automatic relaying was feasible within 25 kHz **[U — FFI-rapport 2009/01894 not verified this session; obtain and read it]**.

**The escapes, and which are open to us.** Every published multi-packet pipelined-flooding design (Splash, P3, Pando) buys spatial reuse with *frequency* diversity — consecutive pipeline layers on non-adjacent channels. On one 25 kHz channel that route is closed; do not go hunting for a clever single-channel scheduling trick, the literature already searched that space. The one escape that fits our constraints is **Controlled Barrage Regions**: source and destination each flood a hop count, every node learns d(s), d(d) and the s–d shortest-path length, and relays only if d(s)+d(d) equals the path length; nodes adjacent to but outside the region suppress, containing the flood to the shortest-path corridor. That needs *three hop counts*, no link-state database and no routes to non-neighbours — and it turns our dead `hdr.dst` field into the thing that makes concurrency possible **[U — mechanism as described in Halford, Courtade & Turck, MILCOM 2012; not verified this session]**. BRN user capacity with CBR scheduling is reported as Θ(√n) for random source-destination pairs and Θ(n/log n) for localised traffic, which tactical voice is **[U]**.

**One correction to how we justify pruning.** In a pipelined flood, pruned and unpruned relays alike transmit in slot n+1 — **pruning frees no slots**. What it buys is energy and a better chance the capture power condition holds. Ni et al.'s 61%/41% marginal-coverage result is about redundancy and collisions, not slots. Meanwhile extra concurrent relays are *sender diversity*, a reliability asset. Mixer's objective is the right one: **steer the number of concurrent transmitters** — too few loses diversity, too many makes the capture power condition unsatisfiable — rather than minimising them. Recast that way, the frontier rule stops being an exception and becomes the principle.

---

## 5. The three things to do next

### 1. Settle the co-slot concurrency question on the bench. Everything else is downstream of it.

BRN works because concurrent identical relays are made to *combine* at the receiver — CPM plus per-node random phase dithering plus a long-blocklength code and MLSE. We chose 4FSK, which has no such mechanism, and then built receiver-decided pruning, a frontier rule and a notional stagger to prevent the very thing that makes the architecture work. That guarantee is unachievable with local heuristics: two candidates that each reach a neighbour the sender does not will both relay in slot n+1, and the frontier rule deliberately *adds* relayers.

Measure, on the CC1200 or in a modelled PHY: with two co-slot relays carrying the identical payload, what received-power ratio does the demodulator need to capture one cleanly, and what happens at 0–3 dB? That number replaces ADR-0008's uncited 10 dB assumption and decides whether the goal is "prune to exactly one relay" (which requires a provably unique election, §2.1, not a heuristic) or "keep several and steer the power spread" (Mixer's objective). Be aware of the CFO/beating risk: with 6 bits of FEC we have no error tolerance to ride through destructive beat periods, which argues for driving concurrency into the capture regime deliberately.

**Read first:** Zimmerling, Mottola & Santini, *Synchronous Transmissions in Low-Power Wireless* — https://arxiv.org/pdf/2001.08557, §2.1 (capture threshold vs capture window) and §3.3.2 (Mixer). Then Leentvaar & Flint (1976) for the FM/FSK capture reference our dB figure should cite.

### 2. Replace hash-to-slot with NAMA election, and add MSF's evidence loop.

Concretely: (a) change beacon slot selection from `hash(my_address) mod pool` to `p_k^t = Rand(k ⊕ t) ⊕ k` evaluated over the existing 1-and-2-hop neighbour table, transmitting iff maximal — one hash per contender per slot, trivially affordable at 15 ms with ≤16 neighbours; (b) treat the address-hashed cell as a *listen* cell, never a blind transmit cell; (c) add per-slot delivery counting — our passive acknowledgement is already the ACK signal — and a relocation rule on the MSF pattern; (d) accept that ownership needs ≥12 colours for a 2-hop neighbourhood of ~11, which does not fit a 4-slot frame, so ownership must live on a **superframe** layered above the 60 ms voice frame. That is not a change to ADR-0008; slot ownership does not have to live on the voice-latency timescale.

**Read first:** Bao & Garcia-Luna-Aceves, *A New Approach to Channel Access Scheduling for Ad Hoc Networks*, MobiCom 2001, Eq. 1–2 — https://web.cs.ucla.edu/classes/fall03/cs218/paper/p210-bao.pdf. Then RFC 9033 §3 (autonomous cells), §6 (relocation), §17 (constants) — https://www.rfc-editor.org/rfc/rfc9033.txt.

### 3. Fix the suppression rule, bound the TTL, and write down the invariant.

Three small changes, one afternoon each. (a) `manet_sched_suppress()` currently implements counter-based C=1; change it to recompute the uncovered neighbour set against the union of all heard transmitters and cancel only when that set is empty — or, if step 1 says we need exactly one relay, replace the whole rule with a deterministic election and delete the suppression path. (b) Cut `MANET_TTL_BITS` so voice TTL is bounded by pipeline depth (4, or 8 if the MCPTT budget is the binding constraint — see below); if reachability beyond that is ever needed, use expanding ring search on a control message, not a longer voice TTL. (c) State the relay rule as properties, RFC 7181 §18.3 style, so it can be tested.

Also fix two documentation errors while you are there. ADR-0002 claims "~20 ms/hop, giving 15 hops in 300 ms" — at 15 ms per slot that is 15 ms/hop, and more importantly 300 ms is the *mouth-to-ear* budget, not the propagation budget. 3GPP TS 22.179 R-6.15.3.2-015: "*The MCPTT Service shall provide a Mouth-to-ear latency (KPI 3) that is less than 300 ms for 95% of all voice bursts*" **[V — verbatim]**, with R-6.15.3.2-012 setting a *separate* 300 ms access-time budget for 95% of requests and R-6.15.3.2-016/017 requiring zero clipped audio at either end **[V]**. Our chain is roughly 60 ms packetisation + ~30 ms mean slot wait + 15 ms/hop + ~60 ms de-jitter + ~30 ms codec ≈ 180 + 15H, giving **H ≤ 8**. And we have no access-time concept at all, because we have no reservation handshake — our PTT-to-go-ahead time is currently undefined, and R-6.15.3.2-016's zero-initial-lost-audio requirement cannot be met without one.

**Read first:** Williams & Camp, MobiHoc'02 §3.4 and §6 — https://www.ceid.upatras.gr/webpages/faculty/manos/files/mobnets/papers_site/compar_brdcst_tecks.pdf — for where self-pruning sits and why. Then RFC 6621 Appendix A (E-CDS) — https://www.rfc-editor.org/rfc/rfc6621.txt — as the target relay-set algorithm.

### After those three

In order: destination-directed containment (CBR) to break the one-conversation-per-network bound and give `hdr.dst` a purpose; per-talk-spurt chain reservation (NBWF's RTS/multi-CTS) if pipelining is kept, since that is the only published reconciliation of "voice must start instantly" with "relays need scheduled slots"; and Babel (RFC 8966, Standards Track, MIT reference implementation ~400 KB of C) for routes to non-neighbours, whose rare-periodic-plus-triggered update model is the only affordable control-plane shape at 19.2 kbps **[V — feasibility condition, "Route selection MUST NOT take seqnos into account", and Appendix A.3 hysteresis all confirmed]**.

### Two housekeeping items

- **Freedom to operate.** TrellisWare's patent estate covers spatial pipelining directly (US 8,873,391, priority 2012-05-24; US 8,897,158; US 9,054,822). If this is ever a product, get an FTO opinion before writing more MAC code.
- **Licence landmine.** Reticulum is *not* MIT — its custom licence forbids use in any system that "includes amongst its functions the ability to purposefully do harm to human beings." If MANET-VHF has any defence or force-protection application, RNS-derived code is off the table. Liftable: MeshCore (MIT), babeld (MIT), OpenWSN (BSD-3), olsrd/OONF (BSD-3), Codec2 (LGPL-2.1, already a dependency). Idea-only unless we go copyleft: Meshtastic, RNode, batman-adv, libm17, Direwolf.

### Unverified claims flagged for follow-up

TETRA's three-timeslot DM-REP lag and the 1-call/2-call type distinction (ETSI returned 403 — download EN 300 396-2 and -4 manually); NBWF's 9 × 22.5 ms frame and the FFI feasibility caveat (obtain FFI-rapport 2009/01894 — this is the most important document on the list and nobody has read it end to end); TrellisWare's M≥3 rule and the CBR mechanism; USAP's D²+1 sizing and Blocked() equations; Glossy's optimality claim; Meshtastic's and RNode's constants; and the ARDC-funded OpenMesh Voice Network project, which appears to be building our exact system on 50 kbps / four TDMA slots and is worth contacting before we spend more on the MAC.