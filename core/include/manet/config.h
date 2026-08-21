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

#define MANET_HEADER_BITS \
    ((long)(2u * MANET_ADDR_BITS + MANET_TYPE_BITS + MANET_SEQ_BITS \
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

#endif /* MANET_CONFIG_H */
