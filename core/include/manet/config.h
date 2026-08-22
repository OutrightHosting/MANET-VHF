/*
 * manet/config.h — compile-time parameters and the slot budget.
 *
 * Every value here is overridable with -D so that a single build of the core can be
 * swept across candidate frame structures. That is the point: OQ-0002 (the slot budget
 * does not close) and OQ-0012 (header field widths) are meant to be answered by
 * compiling and measuring, not by arithmetic in a document.
 *
 * The derived budget at the bottom is enforced by static assertion. If a configuration
 * cannot physically carry its own header and payload, it fails to build.
 *
 * See docs/decisions/0002-tdma-slot-pipelining.md
 *     docs/decisions/0007-packet-switched-frame-architecture.md
 *     docs/open-questions.md#oq-0002
 */
#ifndef MANET_CONFIG_H
#define MANET_CONFIG_H

#include <stdint.h>

/*
 * ADR-0006 fixes the core at freestanding C99, and _Static_assert is C11. Use the real
 * thing when the translation unit is compiled as C11 or later — the message text is
 * worth having, since several of these point at open questions — and fall back to the
 * negative-array-size idiom otherwise. The constraint in the ADR stays true either way.
 */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#define MANET_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#else
#define MANET_STATIC_ASSERT_CAT2_(a, b) a##b
#define MANET_STATIC_ASSERT_CAT_(a, b)  MANET_STATIC_ASSERT_CAT2_(a, b)
#define MANET_STATIC_ASSERT(cond, msg) \
    typedef char MANET_STATIC_ASSERT_CAT_(manet_static_assert_, __LINE__)[(cond) ? 1 : -1]
#endif

/* ---------------------------------------------------------------- RF / MAC -- */

/* Gross on-air bit rate. Scaled from DMR and UNVERIFIED — see OQ-0001. Sweep it. */
#ifndef MANET_GROSS_BITRATE_BPS
#define MANET_GROSS_BITRATE_BPS 19200L
#endif

#ifndef MANET_FRAME_DURATION_US
#define MANET_FRAME_DURATION_US 160000L
#endif

#ifndef MANET_SLOTS_PER_FRAME
#define MANET_SLOTS_PER_FRAME 4L
#endif

/*
 * Guard time as a proportion of the slot, in parts per thousand. 83 = 8.3%, the ratio
 * DMR uses (2.5 ms in 30 ms). Must cover transmitter attack and release (EN 300 113
 * tests adjacent channel power during burst transitions), RX->TX turnaround (OQ-0010)
 * and sync uncertainty (OQ-0003). Propagation is negligible: 50 us at 15 km.
 */
#ifndef MANET_GUARD_PERMILLE
#define MANET_GUARD_PERMILLE 83L
#endif

/*
 * Acquisition preamble and sync word, per burst, in 19.2 kbps bit-times.
 *
 * 56 = a 4-bit AGC-settling preamble plus a 24-bit sync word. Note the units: the CC1200
 * sends preamble and sync word as 2-GFSK even when the payload is 4-GFSK (SWRU346B
 * §5.2.1), so each of those 28 transmitted bits costs one symbol — two payload bit-times.
 *
 * Was 154, taken from NBWF (FFI-rapport 2009/01894 §4.3). That figure does not survive
 * reading the source: its own footnote 4 says "This is an estimate", the fields §2.3
 * specifies sum to 5.3 ms rather than 8, and roughly 1.7 ms of it is a Par field
 * signalling which of five PHY modes a burst uses plus a symbol-rate transition — neither
 * of which a single-mode waveform has. Its 1.5 ms sync preamble is a CW tone for
 * frequency extraction, needed because §4.11 requires NBWF work without GNSS. We are
 * GPS-disciplined.
 *
 * Only about 8-10 bits of the saving comes from knowing slot timing. The rest comes from
 * the disciplined frequency reference and from not being multi-mode — WHICH MAKES
 * GPS-DISCIPLINING THE 40 MHz LO A DESIGN COMMITMENT, not an assumption. If the reference
 * free-runs at +/-2 ppm the CC1200 needs TOC_LIMIT >= 1 and 2-4 bytes of preamble, and
 * this number goes back to about 128.
 *
 * Range: 30 best case (11-bit sync, but its false-sync rate is ~1/s and unlikely to
 * survive a bench), 40-56 likely, 128 if the LO free-runs. Bench tests in
 * docs/preamble-budget.md.
 */
#ifndef MANET_SYNC_BITS
#define MANET_SYNC_BITS 56L
#endif

/* -------------------------------------------------------------------- Voice -- */

/*
 * Codec2 3200 over one 160 ms frame. Voice occupies ONE slot per frame, not several:
 * spreading a payload over M slots divides the spatial reuse distance by M, and at M=2 the
 * reuse distance falls to 2 hops, which is the hidden-terminal case that forced four slots
 * in the first place. See ADR-0009.
 */
#ifndef MANET_VOICE_PAYLOAD_BITS
#define MANET_VOICE_PAYLOAD_BITS 512L
#endif

/* ----------------------------------------------------------- Header widths -- */
/* Field presence is fixed by Addendum 01. Field width is OQ-0012, and every bit
 * spent here is a bit of FEC not spent. Straw-man from ADR-0007. */

#ifndef MANET_ADDR_BITS
#define MANET_ADDR_BITS 8u
#endif

#ifndef MANET_TYPE_BITS
#define MANET_TYPE_BITS 4u
#endif

#ifndef MANET_SEQ_BITS
#define MANET_SEQ_BITS 8u
#endif

/*
 * 4 bits, 15 hops.
 *
 * This has been argued both ways in this project and the literature settles it. An
 * earlier note here set 5 bits so "TTL is never what stops the network", on the reasoning
 * that latency degrades gradually and slow contact beats none. True as far as it goes,
 * but it was measured against the wrong budget: 300 ms is the MOUTH-TO-EAR figure from
 * 3GPP TS 22.179, not a propagation allowance, and our chain gives H <= 8 within it.
 *
 * So 15 hops covers the voice bound with headroom, data can use the whole field, and the
 * bit goes back to FEC where OQ-0002 needs it far more. A shorter TTL also contains a
 * looping frame faster — at 15 hops a loop wastes 225 ms of airtime, at 63 nearly a
 * second.
 */
#ifndef MANET_TTL_BITS
#define MANET_TTL_BITS 4u
#endif

#ifndef MANET_PRIO_BITS
#define MANET_PRIO_BITS 2u
#endif

/* ---------------------------------------------------------- Address ranges -- */
/*
 * Destination kind is implied by range rather than carried in flag bits — Addendum 01
 * asks for frugality in width, and this buys individual/group/broadcast semantics for
 * zero extra header bits.
 *
 * This map is derived for an 8-bit space. Changing MANET_ADDR_BITS requires re-deriving
 * it; the static assertions below will fail loudly rather than misbehave quietly.
 */
#define MANET_ADDR_NULL         0x00u
#define MANET_ADDR_HANDHELD_MIN 0x01u
#define MANET_ADDR_HANDHELD_MAX 0x9Fu  /* 159 handhelds */
#define MANET_ADDR_GATEWAY_MIN  0xA0u
#define MANET_ADDR_GATEWAY_MAX  0xBFu  /*  32 gateways  */
#define MANET_ADDR_GROUP_MIN    0xC0u
#define MANET_ADDR_GROUP_MAX    0xEFu  /*  48 groups    */
#define MANET_ADDR_RESERVED_MIN 0xF0u
#define MANET_ADDR_RESERVED_MAX 0xFEu
#define MANET_ADDR_BROADCAST    0xFFu

/* ------------------------------------------------------------------ Derived -- */

#define MANET_SLOT_DURATION_US  (MANET_FRAME_DURATION_US / MANET_SLOTS_PER_FRAME)
#define MANET_GUARD_US          ((MANET_SLOT_DURATION_US * MANET_GUARD_PERMILLE) / 1000L)
#define MANET_BURST_US          (MANET_SLOT_DURATION_US - MANET_GUARD_US)

/* Raw bits the slot would carry with no guard. */
#define MANET_SLOT_RAW_BITS     ((MANET_GROSS_BITRATE_BPS * MANET_SLOT_DURATION_US) / 1000000L)
/* Bits actually on air, after guard. */
#define MANET_SLOT_ONAIR_BITS   ((MANET_GROSS_BITRATE_BPS * MANET_BURST_US) / 1000000L)

/*
 * src (origin) + prev (last hop) + dst + type + seq + ttl + prio.
 *
 * Origin and previous hop are BOTH required and are not the same thing. Origin
 * identifies the frame for duplicate suppression and survives every hop. Previous hop
 * changes at each hop and is what the forwarding rule tests: a node relays only for a
 * neighbour that has selected it. Without it MPR forwarding cannot be implemented at
 * all — see OQ-0018.
 */
#define MANET_HEADER_BITS \
    ((long)(3u * MANET_ADDR_BITS + MANET_TYPE_BITS + MANET_SEQ_BITS \
            + MANET_TTL_BITS + MANET_PRIO_BITS))

#define MANET_HEADER_BYTES ((size_t)((MANET_HEADER_BITS + 7) / 8))

/*
 * What is left for forward error correction once sync, header and voice are paid for.
 *
 * At the ADR-0007 straw-man (4 slots x 15 ms, 19.2 kbps, 34-bit header) this evaluates
 * to 14 — a CRC, not error correction, on a channel with no retransmission. That is
 * OQ-0002 and it is why this constant is exported rather than buried: the simulator
 * reads it, and a configuration sweep can find the ones that close.
 */
#define MANET_FEC_BITS_AVAILABLE \
    (MANET_SLOT_ONAIR_BITS - MANET_SYNC_BITS - MANET_HEADER_BITS - MANET_VOICE_PAYLOAD_BITS)

/* FEC overhead as a percentage of what it protects (header + voice). DMR's ratio is
 * ~47%. Integer arithmetic — ADR-0006 forbids floating point in the core. */
#define MANET_FEC_PERCENT \
    ((MANET_FEC_BITS_AVAILABLE * 100L) / (MANET_HEADER_BITS + MANET_VOICE_PAYLOAD_BITS))

/* ---------------------------------------------------- Neighbourhood sizing -- */

/* Fixed tables, no allocation. Sized for the operational case (12 leaders) with
 * headroom, not for a large deployment. */
#ifndef MANET_MAX_NEIGHBOURS
#define MANET_MAX_NEIGHBOURS 16u
#endif

/*
 * How much better a newcomer must be before it displaces a live neighbour when the table
 * is full.
 *
 * The table was first-come-first-served with no eviction: once full, a radio could never
 * learn a new neighbour however useful, and however stale the entries it was holding.
 * Harmless in a flat cluster -- everyone hears the talker directly, so 30,000 refused
 * entries at 48 radios cost nothing measurable -- but in a dense multi-hop topology the
 * table fills with whoever spoke first rather than whoever matters, and worst-case
 * delivery fell from 95% to 68.2% across 48 radios in twelve groups.
 *
 * Quality is 0..255, so 24 is roughly a tenth of the scale. Displacing on any improvement
 * at all would let two similar neighbours evict each other indefinitely.
 */
#ifndef MANET_NB_EVICT_MARGIN
#define MANET_NB_EVICT_MARGIN 24u
#endif

/* How many neighbours one neighbour may advertise to us. Caps the two-hop view. */
#ifndef MANET_MAX_ADVERTISED
#define MANET_MAX_ADVERTISED 16u
#endif

#ifndef MANET_MAX_TWO_HOP
#define MANET_MAX_TWO_HOP 32u
#endif

/*
 * Beacon interval, in frames. 33 frames is 5.28 s at the live 160 ms frame (132 slots).
 * The 33 was taken from OLSR's default HELLO interval — inherited, NOT derived, and almost certainly wrong for a link three orders
 * of magnitude slower than OLSR was designed for. This is OQ-0004 and Phase 0 is meant
 * to sweep it.
 */
#ifndef MANET_BEACON_INTERVAL_FRAMES
#define MANET_BEACON_INTERVAL_FRAMES 33u
#endif

/* Miss this many beacons and the neighbour is gone. OLSR uses 3. */
/*
 * OLSR uses 3. Raised to 8 because beacons now defer to voice: a radio carrying a
 * transmission holds its beacon, and the hold time must outlast that or its neighbours
 * age it out mid-call. Costs a slower response to someone genuinely walking away.
 */
#ifndef MANET_NB_HOLD_MULTIPLE
#define MANET_NB_HOLD_MULTIPLE 8u
#endif

/* The group this is actually for. Used only to compute beacon overhead. */
#ifndef MANET_TYPICAL_GROUP
#define MANET_TYPICAL_GROUP 12u
#endif

/* How many recently-seen frames a radio remembers, for duplicate suppression. Must be
 * comfortably more than the number of frames in flight across the network at once. */
#ifndef MANET_DEDUP_DEPTH
#define MANET_DEDUP_DEPTH 32u
#endif

/* Link quality below which a radio treats itself as being at the edge of the sender's
 * reach, and relays even when pruning says it adds no coverage. See mpr.c. */
#ifndef MANET_FRONTIER_QUALITY
#define MANET_FRONTIER_QUALITY 250u
#endif

/*
 * Net entry. A radio that has heard nobody has a contention set of one and would win
 * every election, so a group powering on together all transmit in every slot, collide,
 * learn nothing, and never converge. Until it knows of at least this many contenders it
 * contends against a virtual set of this size instead — which is slotted-ALOHA entry,
 * the same shape as AIS §3.3.5 and USAP: back off, claim a slot, then settle into the
 * schedule.
 */
#ifndef MANET_NAMA_MIN_CONTENDERS
#define MANET_NAMA_MIN_CONTENDERS 12u
#endif

/*
 * The superframe: a slower layer above the 160 ms voice frame, where slot ownership lives.
 *
 * Ownership cannot live inside the voice frame. A two-hop neighbourhood of eleven radios
 * needs at least twelve distinct contention contexts and a four-slot frame offers four.
 * Every earlier attempt to reserve control airtime inside the frame failed for that
 * reason, and the failures looked like tuning problems rather than a sizing error.
 *
 * SUPERSEDED AS A RESERVATION RATE by MANET_SIGNAL_SLOT_PERIOD at line 350 of this file,
 * which is 8 — one slot in eight, 12.5% of airtime, not one in thirty-two at 3%. B-15
 * established that 3% was far too little: beacons could not fit and went out over live
 * voice instead. Do not size the reservation from this block; see ADR-0014.
 *
 * The superframe itself is still live and still owns the contention contexts. Which radio
 * may use a given signalling slot is settled by NAMA election, so twelve radios share the
 * slots in a beacon interval without ever colliding.
 */
#ifndef MANET_SUPERFRAME_FRAMES
#define MANET_SUPERFRAME_FRAMES 8u
#endif

#define MANET_SUPERFRAME_SLOTS \
    ((uint64_t)MANET_SUPERFRAME_FRAMES * (uint64_t)MANET_SLOTS_PER_FRAME)

/*
 * How often a slot is reserved for signalling. One slot in this many carries beacons and
 * topology updates, and voice steps over it rather than transmitting in it.
 *
 * This was one slot per superframe (3.1%) and switched off, because a reservation that
 * DROPPED the voice payload landing in it cost nine points of delivery. B-15 established
 * that the reservation was never the problem: with voice stepping over the slot instead of
 * dying in it, the same mechanism takes a seven-position chain from 77.27% to 99.93% at six
 * radios per position, and removes the density penalty entirely.
 *
 * 8 is one slot in eight, 12.5% of airtime, and it is set by the LATENCY budget rather than
 * by delivery. Delivery is 99.93% at every period tried from 4 to 8 -- the density penalty
 * is gone as soon as beacons have anywhere else to go, and buying more signalling airtime
 * than that buys nothing. What differs is mouth-to-ear over seven hops, since voice steps
 * over each reserved slot it meets:
 *
 *   period   airtime   7 hops   mouth-to-ear   links known (42 radios)
 *        4     25.0%   8 slots        540 ms                     93.8%
 *        5     20.0%   8 slots        524 ms                     84.3%
 *        6     16.7%   7 slots        513 ms                     84.4%
 *        7     14.3%   7 slots        506 ms                     61.1%
 *        8     12.5%   6 slots        480 ms                     63.7%
 *
 * ADR-0011 budgets 500 ms and 7 hops. Only 8 fits, and it is the cheapest of the five in
 * airtime as well. It is deliberately a multiple of MANET_SLOTS_PER_FRAME: the reservation
 * then lands on the same voice phase every time, so one phase's talkers step to the next
 * slot and the other three are untouched. A period coprime with the frame spreads the cost
 * but crosses more reserved slots per hop, which is why 7 is both slower and worse-informed
 * than 8.
 *
 * The links-known column is not the trade it looks like. At the twelve-radio design target
 * the mobility gate is unaffected: dispersal converges 1.00 with delivery 1.00 at both
 * period 4 and period 8, against 0.966 with no reservation at all. 63.7% is a 42-radio
 * figure, and delivery there is 99.93% regardless.
 *
 * The scaling law is NBWF's, recorded in docs/nbwf-lessons.md section 5: signalling capacity
 * has to grow with the number of radios that must advertise, not stay pinned at one slot per
 * superframe. Measured here, links known at six radios per position: 23.5% at one slot in 32,
 * 63.7% at one in 8, 93.8% at one in 4. Shortening the beacon interval does NOT substitute --
 * it plateaus near 65% at period 8, because the binding constraint is the supply of reserved
 * slots to elect within, not how often a radio wants to speak.
 */
#ifndef MANET_SIGNAL_SLOT_PERIOD
#define MANET_SIGNAL_SLOT_PERIOD 8u
#endif

/* How many distinct relayers of one frame a radio remembers while deciding whether its
 * own relay is still needed. See manet_mpr_still_needed(). */
#ifndef MANET_HEARD_MAX
#define MANET_HEARD_MAX 6u
#endif

/*
 * TTL a radio stamps on voice it originates.
 *
 * Bounded by LATENCY, not by the field width. The budget is 500 ms mouth-to-ear (OQ-0022,
 * decided): 160 ms packetisation + ~60 ms de-jitter and codec leaves 280 ms of network.
 * A voice frame outliving that occupies the network to deliver audio too late to answer.
 *
 * SEVEN, and it took fixing the per-hop cost to get there. This was 4, justified by a
 * comment reasoning from a 200 ms frame at 50 ms per hop. At 160 ms and 40 ms slots the one
 * hop per slot ADR-0002 promises gives seven — but the implementation was delivering 6.32
 * slots per hop in the hill scenario, because 81.8% of relay attempts lost a NAMA election
 * and waited for another turn.
 *
 * ADR-0011 removes the election for voice relays. A hop now costs exactly one slot, and the
 * measured chain is slightly better than the arithmetic because the first hop is direct and
 * free: 7 hops = 6 slots = 240 ms, mouth-to-ear 460 ms against 500.
 *
 * ADR-0014 adds one step-over: voice skips each reserved signalling slot it meets, so seven
 * hops costs 6.5 slots — 260 ms of slot time and ~480 ms mouth-to-ear. Still inside 500, and
 * the signalling period was chosen by that budget rather than by delivery, which is 99.93%
 * at every period from 4 to 8.
 *
 * The remaining lever is the frame. At 110 ms — which 22.4 kbps with Codec2 2400 buys, both
 * bench questions (OQ-0001) — the same one-slot hop gives TWELVE.
 *
 * Data is not latency-bound and may use the full field.
 */
#ifndef MANET_VOICE_TTL
#define MANET_VOICE_TTL 7u
#endif

/* Largest PDU that can ride in one slot: everything on air except the sync word.
 * Header and FEC are carried inside this. */
#define MANET_MAX_PDU_BITS  (MANET_SLOT_ONAIR_BITS - MANET_SYNC_BITS)
#define MANET_MAX_PDU_BYTES ((size_t)((MANET_MAX_PDU_BITS + 7) / 8))

/* How many slots ahead the transmit scheduler can hold work. Pipelining needs one;
 * the rest is headroom for priority queueing (Addendum 01 s5). */
#ifndef MANET_SCHED_DEPTH
#define MANET_SCHED_DEPTH 4u
#endif

#define MANET_BEACON_INTERVAL_SLOTS \
    ((uint64_t)MANET_BEACON_INTERVAL_FRAMES * (uint64_t)MANET_SLOTS_PER_FRAME)
#define MANET_NB_HOLD_SLOTS \
    (MANET_BEACON_INTERVAL_SLOTS * (uint64_t)MANET_NB_HOLD_MULTIPLE)

/* ------------------------------------------------------- Beacon arithmetic -- */
/*
 * One advertised neighbour costs an address plus a two-bit link code (asymmetric,
 * symmetric, or "I have selected you as my relay"). A beacon carries the frame header,
 * a count, and one entry per neighbour heard.
 *
 * Two different costs follow, and the gap between them is the whole of OQ-0004:
 *
 *   MANET_BEACON_INFO_PERMILLE — what the beacons actually *say*, as a share of channel
 *   capacity. Small.
 *
 *   MANET_BEACON_SLOT_PERMILLE — what they *occupy*. A beacon needs a whole slot, and a
 *   slot is far bigger than a beacon, so this is several times worse. Closing the gap
 *   means packing several beacons into a slot or piggybacking them on voice — neither
 *   of which is designed.
 */
#define MANET_BEACON_ENTRY_BITS ((long)(MANET_ADDR_BITS + 2u))
#define MANET_BEACON_COUNT_BITS 4L
#define MANET_BEACON_BITS \
    (MANET_HEADER_BITS + MANET_BEACON_COUNT_BITS \
     + ((long)(MANET_TYPICAL_GROUP - 1u) * MANET_BEACON_ENTRY_BITS))

/* Channel bits and slots available in one beacon interval. */
#define MANET_INTERVAL_BITS \
    ((MANET_GROSS_BITRATE_BPS * (long)MANET_BEACON_INTERVAL_FRAMES * MANET_FRAME_DURATION_US) \
     / 1000000L)
#define MANET_INTERVAL_SLOTS \
    ((long)MANET_BEACON_INTERVAL_FRAMES * MANET_SLOTS_PER_FRAME)

/* Every node beacons once per interval. Parts per thousand, integer only. */
#define MANET_BEACON_INFO_PERMILLE \
    ((MANET_BEACON_BITS * (long)MANET_TYPICAL_GROUP * 1000L) / MANET_INTERVAL_BITS)
#define MANET_BEACON_SLOT_PERMILLE \
    (((long)MANET_TYPICAL_GROUP * 1000L) / MANET_INTERVAL_SLOTS)

/* ------------------------------------------------------- Budget enforcement -- */

MANET_STATIC_ASSERT(MANET_SLOTS_PER_FRAME >= 2, "pipelining needs at least 2 slots");
MANET_STATIC_ASSERT(MANET_GUARD_US > 0, "guard interval must be non-zero: EN 300 113 burst transitions");
MANET_STATIC_ASSERT(MANET_SLOT_ONAIR_BITS > 0, "no bits on air");

/* The frame must at minimum physically fit. This is a far weaker condition than the
 * frame being *viable* — see MANET_FEC_BITS_AVAILABLE above and OQ-0002.
 *
 * tools/budget.c defines MANET_ALLOW_INFEASIBLE so it can report configurations that
 * do not fit rather than failing to build. Nothing else may define it. */
#ifndef MANET_ALLOW_INFEASIBLE
MANET_STATIC_ASSERT(MANET_FEC_BITS_AVAILABLE > 0,
               "slot budget does not fit sync + header + voice; see OQ-0002");
#endif

MANET_STATIC_ASSERT(MANET_ADDR_BITS == 8u,
               "address range map in this header is derived for an 8-bit space; "
               "re-derive the ranges before changing MANET_ADDR_BITS (OQ-0012)");
MANET_STATIC_ASSERT(MANET_ADDR_HANDHELD_MIN <= MANET_ADDR_HANDHELD_MAX, "address map disordered");
MANET_STATIC_ASSERT(MANET_ADDR_HANDHELD_MAX < MANET_ADDR_GATEWAY_MIN,   "address map disordered");
MANET_STATIC_ASSERT(MANET_ADDR_GATEWAY_MAX  < MANET_ADDR_GROUP_MIN,     "address map disordered");
MANET_STATIC_ASSERT(MANET_ADDR_GROUP_MAX    < MANET_ADDR_RESERVED_MIN,  "address map disordered");
MANET_STATIC_ASSERT(MANET_ADDR_RESERVED_MAX < MANET_ADDR_BROADCAST,     "address map disordered");

MANET_STATIC_ASSERT(MANET_PRIO_BITS >= 2u, "four priority classes are mandated by Addendum 01 s5");
MANET_STATIC_ASSERT(MANET_TYPE_BITS >= 4u, "frame type space must leave room to grow");
MANET_STATIC_ASSERT(MANET_HEADER_BITS <= 64, "header implausibly large; check field widths");
MANET_STATIC_ASSERT(MANET_MAX_PDU_BITS > MANET_HEADER_BITS, "no room for a PDU after sync");
MANET_STATIC_ASSERT(MANET_SCHED_DEPTH >= 2u, "pipelining needs to schedule at least one slot ahead");
MANET_STATIC_ASSERT(MANET_MAX_NEIGHBOURS >= MANET_TYPICAL_GROUP - 1u,
                    "neighbour table cannot hold a fully-connected group");
MANET_STATIC_ASSERT(MANET_BEACON_BITS <= MANET_MAX_PDU_BITS,
                    "a beacon for a typical group does not fit in one slot");
MANET_STATIC_ASSERT((MANET_FRAME_DURATION_US % MANET_SLOTS_PER_FRAME) == 0,
                    "frame does not divide evenly into slots");

#endif /* MANET_CONFIG_H */
