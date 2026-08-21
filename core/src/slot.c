#include "manet/slot.h"

/* ------------------------------------------------------------------ timing -- */

void manet_slot_from_number(uint64_t number, manet_slot_pos_t *pos)
{
    const uint64_t per_frame = (uint64_t)MANET_SLOTS_PER_FRAME;

    if (pos == NULL) {
        return;
    }
    pos->number   = number;
    pos->frame    = number / per_frame;
    pos->index    = (uint8_t)(number % per_frame);
    pos->start_us = number * (uint64_t)MANET_SLOT_DURATION_US;
}

void manet_slot_at(uint64_t t_us, manet_slot_pos_t *pos)
{
    manet_slot_from_number(t_us / (uint64_t)MANET_SLOT_DURATION_US, pos);
}

uint64_t manet_slot_start_us(uint64_t number)
{
    return number * (uint64_t)MANET_SLOT_DURATION_US;
}

uint64_t manet_slot_burst_end_us(uint64_t number)
{
    return manet_slot_start_us(number) + (uint64_t)MANET_BURST_US;
}

bool manet_slot_is_control(uint64_t number)
{
    /* The last slot of each superframe. Any fixed position would do; the last keeps it
     * clear of the frame boundary where a talker starts a burst. */
    return (number % MANET_SUPERFRAME_SLOTS) == (MANET_SUPERFRAME_SLOTS - 1u);
}

uint64_t manet_slot_next_voice(uint64_t number)
{
    return manet_slot_is_control(number) ? (number + 1u) : number;
}

bool manet_slot_in_burst(uint64_t t_us)
{
    return (t_us % (uint64_t)MANET_SLOT_DURATION_US) < (uint64_t)MANET_BURST_US;
}

/* --------------------------------------------------------------- scheduler -- */

static manet_sched_entry_t *entry_for_slot(manet_sched_t *s, uint64_t slot)
{
    size_t i;
    for (i = 0u; i < (size_t)MANET_SCHED_DEPTH; i++) {
        if (s->entries[i].occupied && s->entries[i].slot_number == slot) {
            return &s->entries[i];
        }
    }
    return NULL;
}

static manet_sched_entry_t *free_entry(manet_sched_t *s)
{
    size_t i;
    for (i = 0u; i < (size_t)MANET_SCHED_DEPTH; i++) {
        if (!s->entries[i].occupied) {
            return &s->entries[i];
        }
    }
    return NULL;
}

/*
 * A radio has one transmitter, so a slot holds one PDU. When two things want the same
 * slot the higher priority class wins outright — Addendum 01 s5: emergency pre-empts
 * everything, voice pre-empts data, and voice is never queued behind bulk traffic.
 * Equal priority does not displace: first claim holds, which keeps a voice stream
 * intact rather than letting each new frame evict the last.
 */
static manet_status_t place(manet_sched_t *s, const manet_pdu_t *pdu, uint64_t slot)
{
    manet_sched_entry_t *e = entry_for_slot(s, slot);

    if (e != NULL) {
        if (pdu->hdr.prio < e->pdu.hdr.prio) {
            e->pdu = *pdu;
            return MANET_OK;
        }
        return MANET_ERR_BUFFER;
    }

    e = free_entry(s);
    if (e == NULL) {
        return MANET_ERR_BUFFER;
    }
    e->occupied    = true;
    e->slot_number = slot;
    e->pdu         = *pdu;
    return MANET_OK;
}

void manet_sched_init(manet_sched_t *s)
{
    size_t i;
    if (s == NULL) {
        return;
    }
    for (i = 0u; i < (size_t)MANET_SCHED_DEPTH; i++) {
        s->entries[i].occupied = false;
    }
    s->last_taken = 0u;
    s->has_taken  = false;
}

manet_status_t manet_sched_relay(manet_sched_t *s, const manet_pdu_t *pdu,
                                 uint64_t rx_slot, manet_addr_t me)
{
    manet_pdu_t next;

    if (s == NULL || pdu == NULL) {
        return MANET_ERR_NULL_ARG;
    }

    next = *pdu;

    /* Loop prevention happens here, before the frame is committed to a slot. A frame
     * whose TTL expires is dropped rather than scheduled. */
    if (!manet_header_ttl_decrement(&next.hdr)) {
        return MANET_ERR_TTL_EXPIRED;
    }

    /* We are now the previous hop. The origin is untouched — it is what identifies the
     * frame for duplicate suppression all the way across the network. */
    manet_header_set_prev(&next.hdr, me);

    /* The pipelining rule, and the reason this system works at all: the very next slot,
     * not the next frame. Crossing a frame boundary is not a special case — slot
     * numbers are monotonic, so a transmission advances one hop per slot continuously.
     * See ADR-0002. */
    return place(s, &next, rx_slot + 1u);
}

manet_status_t manet_sched_originate(manet_sched_t *s, const manet_pdu_t *pdu, uint64_t slot)
{
    if (s == NULL || pdu == NULL) {
        return MANET_ERR_NULL_ARG;
    }
    return place(s, pdu, slot);
}

bool manet_sched_take(manet_sched_t *s, uint64_t slot, manet_pdu_t *out)
{
    bool   found = false;
    size_t i;

    if (s == NULL) {
        return false;
    }

    for (i = 0u; i < (size_t)MANET_SCHED_DEPTH; i++) {
        manet_sched_entry_t *e = &s->entries[i];

        if (!e->occupied) {
            continue;
        }
        if (e->slot_number == slot) {
            if (out != NULL) {
                *out = e->pdu;
            }
            e->occupied   = false;
            s->last_taken = slot;
            s->has_taken  = true;
            found         = true;
        } else if (e->slot_number < slot) {
            /* Its moment passed while the radio was doing something else. Sending it
             * late would arrive out of order in a voice stream and collide with
             * whatever now owns that slot elsewhere on the chain. Drop it. */
            e->occupied = false;
        } else {
            /* still in the future */
        }
    }
    return found;
}

bool manet_sched_suppress(manet_sched_t *s, manet_addr_t src, uint8_t seq)
{
    bool   any = false;
    size_t i;

    if (s == NULL) {
        return false;
    }

    for (i = 0u; i < (size_t)MANET_SCHED_DEPTH; i++) {
        manet_sched_entry_t *e = &s->entries[i];

        /* hdr.src is the ORIGIN of the frame, not whoever last relayed it — which is
         * precisely what makes passive acknowledgement work. Two nodes queued to relay
         * the same frame hold the same src and seq, so hearing either one go out
         * cancels the other. */
        if (e->occupied && e->pdu.hdr.src == src && e->pdu.hdr.seq == seq) {
            e->occupied = false;
            any         = true;
        }
    }
    return any;
}

void manet_sched_flush(manet_sched_t *s)
{
    size_t i;
    if (s == NULL) {
        return;
    }
    for (i = 0u; i < (size_t)MANET_SCHED_DEPTH; i++) {
        s->entries[i].occupied = false;
    }
}

size_t manet_sched_depth(const manet_sched_t *s)
{
    size_t n = 0u;
    size_t i;

    if (s == NULL) {
        return 0u;
    }
    for (i = 0u; i < (size_t)MANET_SCHED_DEPTH; i++) {
        if (s->entries[i].occupied) {
            n++;
        }
    }
    return n;
}
