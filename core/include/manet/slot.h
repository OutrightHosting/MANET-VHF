/*
 * manet/slot.h — TDMA slot timing and the pipelining rule.
 *
 * Two things live here, and they are deliberately separate:
 *
 *   1. Timing arithmetic. Pure functions over a shared timebase. Given a time in
 *      microseconds, which slot is it, when does that slot's burst start and end.
 *      No state, no node identity.
 *
 *   2. The transmit scheduler. Per-node state holding what this radio intends to send
 *      and in which slot, with the pipelining rule as its defining behaviour:
 *      something received in slot n is relayed in slot n+1.
 *
 * What is NOT here: the decision of *whether* to relay. That is duplicate suppression
 * and MPR selection, and it belongs to those modules. This one decides *when*, given
 * that the answer is yes. See docs/decisions/0002-tdma-slot-pipelining.md.
 *
 * Time is a parameter throughout — this module owns no clock (ADR-0006).
 */
#ifndef MANET_SLOT_H
#define MANET_SLOT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "manet/config.h"
#include "manet/frame.h"

/* ------------------------------------------------------------------ timing -- */

/*
 * Slot numbers are monotonic from the timebase epoch and never wrap in any plausible
 * deployment: at 15 ms a uint64 lasts longer than the universe has existed. Frame and
 * index are derived views of the same number.
 */
typedef struct {
    uint64_t number;   /* monotonic slot number since epoch          */
    uint64_t frame;    /* number / MANET_SLOTS_PER_FRAME             */
    uint8_t  index;    /* number % MANET_SLOTS_PER_FRAME             */
    uint64_t start_us; /* absolute start of the slot                 */
} manet_slot_pos_t;

/* Which slot contains this instant. */
void manet_slot_at(uint64_t t_us, manet_slot_pos_t *pos);

/* Expand a slot number into its full position. */
void manet_slot_from_number(uint64_t number, manet_slot_pos_t *pos);

/* Absolute start of a slot number. */
uint64_t manet_slot_start_us(uint64_t number);

/*
 * The burst occupies the slot minus the guard interval, and the guard sits at the END
 * of the slot. A transmitter therefore keys up at the slot boundary and must be off air
 * before the next one — which is what gives a relaying node its RX->TX turnaround, and
 * what EN 300 113 measures when it tests adjacent channel power during burst
 * transitions. See OQ-0010.
 */
uint64_t manet_slot_burst_end_us(uint64_t number);

/* True if t_us falls inside a burst rather than a guard interval. */
bool manet_slot_in_burst(uint64_t t_us);

/* ----------------------------------------------------------------- the PDU -- */

typedef struct {
    manet_header_t hdr;
    uint8_t        payload[MANET_MAX_PDU_BYTES];
    uint16_t       payload_len; /* bytes of payload after the header */
} manet_pdu_t;

/* --------------------------------------------------------------- scheduler -- */

typedef struct {
    bool        occupied;
    uint64_t    slot_number;
    manet_pdu_t pdu;
} manet_sched_entry_t;

typedef struct {
    manet_sched_entry_t entries[MANET_SCHED_DEPTH];
    uint64_t            last_taken; /* slot number of the last transmission taken   */
    bool                has_taken;
} manet_sched_t;

void manet_sched_init(manet_sched_t *s);

/*
 * Schedule a relay of something heard in rx_slot. This is the pipelining rule: the
 * relay goes out in rx_slot + 1, which is the next slot in time regardless of whether
 * that crosses a frame boundary — a transmission advances one hop per slot,
 * continuously.
 *
 * The caller has already decided this frame should be relayed. TTL is decremented here,
 * a frame that expires is not scheduled, and `me` is stamped as the new previous hop —
 * which is the whole of what relaying changes in a frame. The origin is left alone.
 *
 * Returns MANET_OK if scheduled, MANET_ERR_TTL_EXPIRED if the frame died, or
 * MANET_ERR_BUFFER if the target slot is already claimed by something of equal or
 * higher priority.
 */
manet_status_t manet_sched_relay(manet_sched_t *s, const manet_pdu_t *pdu,
                                 uint64_t rx_slot, manet_addr_t me);

/*
 * Schedule a locally originated transmission in a specific slot. Same priority rules.
 */
manet_status_t manet_sched_originate(manet_sched_t *s, const manet_pdu_t *pdu, uint64_t slot);

/*
 * Take whatever is due to transmit in this slot, if anything. Removes it from the
 * schedule. Returns false if this radio has nothing to send — which is the common case,
 * and the case that makes a clustered group behave exactly like simplex.
 */
bool manet_sched_take(manet_sched_t *s, uint64_t slot, manet_pdu_t *out);

/*
 * Passive acknowledgement: this radio overheard someone else relay the same frame, so
 * its own queued relay is redundant and must be dropped. Matching is on origin and
 * sequence, which is what the header carries for exactly this purpose.
 *
 * Returns true if something was actually suppressed.
 */
bool manet_sched_suppress(manet_sched_t *s, manet_addr_t src, uint8_t seq);

/* Drop everything scheduled at or before this slot — used when a node loses sync or
 * a stream ends. */
void manet_sched_flush(manet_sched_t *s);

/* How many entries are currently queued. For tests and instrumentation. */
size_t manet_sched_depth(const manet_sched_t *s);

#endif /* MANET_SLOT_H */
