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
    e->heard_count = 0u;
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

static manet_sched_entry_t *find_frame(manet_sched_t *s, manet_addr_t src, uint8_t seq)
{
    size_t i;
    for (i = 0u; i < (size_t)MANET_SCHED_DEPTH; i++) {
        if (s->entries[i].occupied && s->entries[i].pdu.hdr.src == src &&
            s->entries[i].pdu.hdr.seq == seq) {
            return &s->entries[i];
        }
    }
    return NULL;
}

size_t manet_sched_note_relay(manet_sched_t *s, manet_addr_t src, uint8_t seq,
                              manet_addr_t from)
{
    manet_sched_entry_t *e;
    uint8_t              i;

    if (s == NULL) {
        return 0u;
    }
    e = find_frame(s, src, seq);
    if (e == NULL) {
        return 0u;
    }
    for (i = 0u; i < e->heard_count; i++) {
        if (e->heard[i] == from) {
            return (size_t)e->heard_count;   /* already counted */
        }
    }
    if (e->heard_count < (uint8_t)MANET_HEARD_MAX) {
        e->heard[e->heard_count] = from;
        e->heard_count++;
    }
    return (size_t)e->heard_count;
}

size_t manet_sched_heard(const manet_sched_t *s, manet_addr_t src, uint8_t seq,
                         manet_addr_t *out, size_t cap)
{
    size_t i;
    uint8_t j;

    if (s == NULL) {
        return 0u;
    }
    for (i = 0u; i < (size_t)MANET_SCHED_DEPTH; i++) {
        const manet_sched_entry_t *e = &s->entries[i];
        if (!e->occupied || e->pdu.hdr.src != src || e->pdu.hdr.seq != seq) {
            continue;
        }
        for (j = 0u; j < e->heard_count && (size_t)j < cap; j++) {
            out[j] = e->heard[j];
        }
        return (size_t)e->heard_count;
    }
    return 0u;
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

/* ---------------------------------------------------------- origination phase -- */

/*
 * Avalanche mix, same construction as nama.c. A plain multiply-and-shift is not enough
 * here: consecutive addresses must not land on consecutive phases, or a group issued
 * sequential radios gets a pathological assignment on day one.
 */
static uint32_t phase_mix32(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7FEB352DuL;
    x ^= x >> 15;
    x *= 0x846CA68BuL;
    x ^= x >> 16;
    return x;
}

uint8_t manet_voice_phase(manet_addr_t addr)
{
    return (uint8_t)(phase_mix32((uint32_t)addr) % (uint32_t)MANET_SLOTS_PER_FRAME);
}

bool manet_voice_phase_free(uint32_t occupied)
{
    uint32_t all = (MANET_SLOTS_PER_FRAME >= 32)
                 ? 0xFFFFFFFFu
                 : (((uint32_t)1u << MANET_SLOTS_PER_FRAME) - 1u);
    return (occupied & all) != all;
}

uint8_t manet_voice_phase_avoiding(manet_addr_t addr, uint32_t occupied)
{
    uint8_t base = manet_voice_phase(addr);
    unsigned i;

    if (!manet_voice_phase_free(occupied)) {
        return base;   /* saturated — report the ceiling rather than hide it */
    }

    for (i = 0; i < (unsigned)MANET_SLOTS_PER_FRAME; i++) {
        uint8_t ph = (uint8_t)((base + i) % (unsigned)MANET_SLOTS_PER_FRAME);
        if ((occupied & ((uint32_t)1u << ph)) == 0u) {
            return ph;
        }
    }
    return base;   /* unreachable given the check above; kept total */
}
