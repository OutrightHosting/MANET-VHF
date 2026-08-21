/*
 * manet/frame.h — on-air frame header.
 *
 * Every transmission is an addressed, typed frame; voice is one type among several.
 * See docs/addendum-01-packet-architecture.md and ADR-0007.
 *
 * This module owns the header and nothing else. It has no knowledge of any payload,
 * including voice. Only `dispatch` may switch on frame type.
 *
 * --------------------------------------------------------------------------------
 * The rule that matters here: unpack is structural, validate is semantic, and the
 * relay path uses only unpack.
 *
 * A node must relay a frame whose type it does not recognise and whose destination is
 * a node class it has never heard of. If relaying depended on validation, a network
 * could never be upgraded a handful of radios at a time — an old node would silently
 * black-hole every new frame type, and the failure would look like a coverage problem.
 * So: manet_header_unpack() accepts any bit pattern that fits, and rejects nothing on
 * semantic grounds. manet_header_validate() is for originating a frame and for
 * deciding local delivery. Never gate forwarding on it.
 * --------------------------------------------------------------------------------
 *
 * Pure functions. No state.
 */
#ifndef MANET_FRAME_H
#define MANET_FRAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "manet/addr.h"
#include "manet/config.h"

/* Frame types. Values 0x8-0xF reserved — ADR-0007. */
typedef enum {
    MANET_FRAME_VOICE     = 0x0, /* voice payload, in-stream                     */
    MANET_FRAME_VOICE_END = 0x1, /* stream terminator                            */
    MANET_FRAME_BEACON    = 0x2, /* neighbour discovery                          */
    MANET_FRAME_TC        = 0x3, /* topology control                             */
    MANET_FRAME_EMERGENCY = 0x4, /* emergency / man-down                         */
    MANET_FRAME_TEXT      = 0x5, /* text message                                 */
    MANET_FRAME_POSITION  = 0x6, /* position report                              */
    MANET_FRAME_CONFIG    = 0x7, /* remote configuration                         */
    MANET_FRAME_TYPE_MAX  = 0x7  /* highest defined; above this is reserved      */
} manet_frame_type_t;

/* Priority classes — Addendum 01 s5. Lower value pre-empts higher. */
typedef enum {
    MANET_PRIO_EMERGENCY  = 0,   /* pre-empts everything                         */
    MANET_PRIO_VOICE      = 1,   /* pre-empts data; never queued behind bulk     */
    MANET_PRIO_SIGNALLING = 2,   /* beacons, topology updates                    */
    MANET_PRIO_DATA       = 3    /* best-effort, yields to all above             */
} manet_priority_t;

typedef enum {
    MANET_OK = 0,
    MANET_ERR_NULL_ARG,
    MANET_ERR_BUFFER,        /* buffer too small, or bits ran out                */
    MANET_ERR_FIELD_RANGE,   /* a field does not fit its configured width        */
    MANET_ERR_BAD_SOURCE,    /* source is not an individual address              */
    MANET_ERR_BAD_DEST,      /* destination is null                              */
    MANET_ERR_RESERVED_TYPE, /* frame type is in the reserved range              */
    MANET_ERR_TTL_EXPIRED
} manet_status_t;

/*
 * Unpacked header. Wire layout is src, prev, dst, type, seq, ttl, prio — MSB first.
 *
 * `src` is the ORIGIN and never changes as a frame crosses the network. `prev` is
 * whoever transmitted this particular copy, and is rewritten at every hop. Duplicate
 * suppression keys on src; the forwarding decision keys on prev. Confusing the two
 * produces a mesh that either loops or refuses to relay.
 */
typedef struct {
    manet_addr_t src;
    manet_addr_t prev;
    manet_addr_t dst;
    uint8_t      type; /* manet_frame_type_t, or an unrecognised reserved value */
    uint8_t      seq;
    uint8_t      ttl;
    uint8_t      prio; /* manet_priority_t */
} manet_header_t;

/* Maximum TTL representable in the configured field width. */
#define MANET_TTL_MAX ((uint8_t)((1u << MANET_TTL_BITS) - 1u))

/* Serialise. Writes exactly MANET_HEADER_BITS bits, MSB first, into the low bits of
 * buf. Bits beyond the header within the final byte are left untouched.
 *
 * Validates field widths but NOT semantics — call manet_header_validate() first when
 * originating. */
manet_status_t manet_header_pack(const manet_header_t *h, uint8_t *buf, size_t cap_bytes);

/* Stamp this radio as the previous hop. Called by a relay before retransmitting. */
void manet_header_set_prev(manet_header_t *h, manet_addr_t me);

/* Deserialise. Structural only: succeeds for any bit pattern of sufficient length,
 * including unrecognised frame types and reserved destination addresses. See the note
 * at the top of this file — this is what makes the network upgradable. */
manet_status_t manet_header_unpack(manet_header_t *h, const uint8_t *buf, size_t cap_bytes);

/* Semantic checks, for originating a frame and for deciding local delivery.
 * NEVER gate forwarding on this. */
manet_status_t manet_header_validate(const manet_header_t *h);

/* True if the type is one this build recognises. A false result is not an error —
 * it means relay it, do not deliver it. */
bool manet_frame_type_is_known(uint8_t type);

/* The priority class a given frame type belongs to, per ADR-0007. Used when
 * originating; the field is carried on the wire so a receiver never has to infer it. */
manet_priority_t manet_frame_default_priority(uint8_t type);

/* Loop prevention. Decrements in place; returns false when the frame has expired and
 * must be dropped rather than relayed. */
bool manet_header_ttl_decrement(manet_header_t *h);

#endif /* MANET_FRAME_H */
