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

/*
 * True if this slot is reserved for control traffic. Voice never transmits in one.
 *
 * The reservation is what gives beacons airtime that a talker cannot take, and the
 * superframe is what makes it affordable — one slot in thirty-two rather than one in four,
 * which is the difference between 3% of the channel and 25% of it.
 *
 * PROVIDED BUT DELIBERATELY NOT USED. Measured against NAMA election alone, reserving
 * control slots costs more than it buys at every superframe length tried:
 *
 *              cluster relays  mobility worst  static chain (woodland)
 *   NAMA only              0           90.3%                    92.5%
 *   + 1 slot in 8         99           88.0%                    83.5%
 *   + 1 slot in 32      8351           89.0%                    83.5%
 *
 * The reason is the same one that defeated an earlier attempt at fixed reserved slots: a
 * reservation punches a hole in the relay pipeline, the payload that needed that slot is
 * delayed, and the delay cascades into the payload behind. NAMA already prevents
 * beacon-against-beacon collisions without taking any airtime at all, which was the
 * reservation's whole purpose.
 *
 * Kept because the primitive is correct and tested, and because a configuration with more
 * slack — a lower vocoder rate, or a measured bit rate above 19.2 kbps — could afford it.
 * Do not wire it in without re-measuring.
 */
bool manet_slot_is_control(uint64_t number);

/* The next slot at or after `number` that voice may use. */
uint64_t manet_slot_next_voice(uint64_t number);

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
    bool         occupied;
    uint64_t     slot_number;
    manet_pdu_t  pdu;
    /* Which radios have been heard relaying this frame. Cancelling on the FIRST of them
     * is Ni et al.'s counter-based scheme at C=1 — the most aggressive setting of a family
     * MobiCom'99 shows cannot guarantee reachability. Keeping the set lets the decision be
     * made on coverage instead. */
    manet_addr_t heard[MANET_HEARD_MAX];
    uint8_t      heard_count;
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
 * Passive acknowledgement, first half: record that `from` was heard relaying this frame.
 *
 * Recording is NOT cancelling. Whether this radio's own relay is still needed is a
 * coverage question — see manet_mpr_still_needed() — and answering it requires knowing
 * every radio heard so far, not just the most recent. Cancelling on the first duplicate
 * is Ni et al.'s counter-based scheme with C=1, which MobiCom'99 shows cannot guarantee
 * that every node is reached. On a voice network that failure is a radio going silent
 * with no error indication.
 *
 * Returns the number of distinct relayers now recorded, or 0 if the frame is not queued.
 */
size_t manet_sched_note_relay(manet_sched_t *s, manet_addr_t src, uint8_t seq,
                              manet_addr_t from);

/* Read back who has been heard relaying a queued frame. */
size_t manet_sched_heard(const manet_sched_t *s, manet_addr_t src, uint8_t seq,
                         manet_addr_t *out, size_t cap);

/*
 * Passive acknowledgement, second half: drop this radio's queued relay of a frame.
 *
 * Call only once coverage says it adds nothing. Returns true if something was dropped.
 */
bool manet_sched_suppress(manet_sched_t *s, manet_addr_t src, uint8_t seq);

/* Drop everything scheduled at or before this slot — used when a node loses sync or
 * a stream ends. */
void manet_sched_flush(manet_sched_t *s);

/* How many entries are currently queued. For tests and instrumentation. */
size_t manet_sched_depth(const manet_sched_t *s);

/* ---------------------------------------------------------- origination phase --

   Which slot of the frame a radio BEGINS a talkspurt in. Distinct from relaying:
   a relay transmits in the slot after it received, which the scheduler above owns.
   This is where a stream enters the network.

   It lived only in the Python harness until now, which by ADR-0006's own rule meant
   the most fundamental MAC decision in the system was not built.

   THE PIGEONHOLE, STATED PLAINLY. There are MANET_SLOTS_PER_FRAME phases and more
   radios than that, so distinct phases for every radio are IMPOSSIBLE — with four
   phases and twelve radios roughly one talker pair in five collides, and no hash
   fixes that. What matters is narrower and achievable: no two radios talking AT THE
   SAME TIME may share a phase, and there are at most MANET_SLOTS_PER_FRAME of those.

   Two radios that share a phase and key up together originate in the same slot every
   frame, permanently — not a glancing collision but a standing one. Under barrage
   relaying that is measured as one stream at 95% and the other at 0%. */

/* This radio's default phase: a hash of its address, so every neighbour can derive it
   from `src` in a header with no signalling. */
uint8_t manet_voice_phase(manet_addr_t addr);

/*
 * The phase to actually originate in, given which phases are already carrying someone
 * else's stream. `occupied` is a bitmask, bit n set meaning phase n is in use by
 * another source — build it from what this radio has recently heard.
 *
 * Returns the default phase when it is free. Otherwise walks forward deterministically
 * to the first free one, so two radios resolving the same collision from the same
 * information reach the same answer without exchanging anything.
 *
 * When every phase is occupied the network is at its structural limit and the default
 * is returned: there is nowhere to move to, and pretending otherwise would hide the
 * capacity ceiling rather than report it. Callers wanting to know should test
 * manet_voice_phase_free().
 */
uint8_t manet_voice_phase_avoiding(manet_addr_t addr, uint32_t occupied);

/* Whether any phase is free at all. False means the frame is saturated with concurrent
   talkers and a new talkspurt has no collision-free slot to start in. */
bool manet_voice_phase_free(uint32_t occupied);

#endif /* MANET_SLOT_H */
