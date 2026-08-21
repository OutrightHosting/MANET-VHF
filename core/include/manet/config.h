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
#define MANET_FRAME_DURATION_US 60000L
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

/* Sync word / preamble. ASSUMED, not derived — DMR spends 48. Largest unverified
 * number in the budget. Owned by the modem layer, not by `frame`, but it competes
 * for the same bits so it is accounted here. */
#ifndef MANET_SYNC_BITS
#define MANET_SYNC_BITS 24L
#endif

/* -------------------------------------------------------------------- Voice -- */

/* Codec2 3200 over one 60 ms frame. See ADR-0004. */
#ifndef MANET_VOICE_PAYLOAD_BITS
#define MANET_VOICE_PAYLOAD_BITS 192L
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
 * 5 bits, 31 hops. Sized so TTL is never what stops the network.
 *
 * 4 bits caps at 15 hops, and an earlier note here argued that was fine because voice
 * stops being conversational around there anyway. That reasoning was wrong: latency
 * degrades gradually, and at the far end of a long chain the alternative to a slow
 * conversation is no contact at all. Extending reach is the entire point of a mesh, so
 * the loop-prevention counter must not be the thing that truncates it.
 *
 * Not larger, though: a looping frame at 63 hops wastes nearly a second of airtime
 * before it dies, and every bit here is a bit of FEC (OQ-0002).
 */
#ifndef MANET_TTL_BITS
#define MANET_TTL_BITS 5u
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

/* How many neighbours one neighbour may advertise to us. Caps the two-hop view. */
#ifndef MANET_MAX_ADVERTISED
#define MANET_MAX_ADVERTISED 16u
#endif

#ifndef MANET_MAX_TWO_HOP
#define MANET_MAX_TWO_HOP 32u
#endif

/*
 * Beacon interval, in frames. 33 frames is ~2 s at 60 ms, which is OLSR's default HELLO
 * interval — inherited, NOT derived, and almost certainly wrong for a link three orders
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
