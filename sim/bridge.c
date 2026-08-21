/*
 * bridge.c — plumbing between the C protocol core and the Python harness.
 *
 * Platform code, NOT protocol logic. Nothing here decides anything: it exposes the
 * core's compile-time configuration, the sizes of its opaque state objects, and
 * scalar accessors so Python never has to mirror a struct layout.
 *
 * The configuration getters exist to kill a specific class of bug. If Python assumed
 * four slots per frame while the core was compiled with three, every result would be
 * quietly wrong. Python asks the library what it was built with.
 */
#include <string.h>

#include "manet/dedup.h"
#include "manet/nama.h"
#include "manet/frame.h"
#include "manet/mpr.h"
#include "manet/neighbour.h"
#include "manet/slot.h"

#define EXPORT __attribute__((visibility("default")))

/* ---------------------------------------------------------- configuration -- */

EXPORT long mb_cfg(int which)
{
    switch (which) {
    case 0:  return MANET_SLOTS_PER_FRAME;
    case 1:  return MANET_SLOT_DURATION_US;
    case 2:  return MANET_FRAME_DURATION_US;
    case 3:  return MANET_GUARD_US;
    case 4:  return MANET_BURST_US;
    case 5:  return MANET_GROSS_BITRATE_BPS;
    case 6:  return MANET_SLOT_ONAIR_BITS;
    case 7:  return MANET_HEADER_BITS;
    case 8:  return MANET_MAX_PDU_BITS;
    case 9:  return MANET_FEC_BITS_AVAILABLE;
    case 10: return MANET_FEC_PERCENT;
    case 11: return MANET_BEACON_BITS;
    case 12: return (long)MANET_BEACON_INTERVAL_FRAMES;
    case 13: return (long)MANET_NB_HOLD_SLOTS;
    case 14: return (long)MANET_MAX_NEIGHBOURS;
    case 15: return (long)MANET_TTL_MAX;
    case 16: return (long)MANET_VOICE_PAYLOAD_BITS;
    case 17: return (long)MANET_SYNC_BITS;
    default: return -1;
    }
}

/* --------------------------------------------------------------- sizeofs -- */

EXPORT unsigned long mb_sizeof(int which)
{
    switch (which) {
    case 0:  return (unsigned long)sizeof(manet_sched_t);
    case 1:  return (unsigned long)sizeof(manet_nb_table_t);
    case 2:  return (unsigned long)sizeof(manet_mpr_set_t);
    case 3:  return (unsigned long)sizeof(manet_pdu_t);
    case 4:  return (unsigned long)sizeof(manet_dedup_t);
    default: return 0ul;
    }
}

/* ------------------------------------------------------------------- PDU -- */

EXPORT void mb_pdu_set(void *p, unsigned src, unsigned prev, unsigned dst,
                       unsigned type, unsigned seq, unsigned ttl, unsigned prio)
{
    manet_pdu_t *pdu = (manet_pdu_t *)p;
    memset(pdu, 0, sizeof *pdu);
    pdu->hdr.src      = (manet_addr_t)src;
    pdu->hdr.prev     = (manet_addr_t)prev;
    pdu->hdr.dst      = (manet_addr_t)dst;
    pdu->hdr.type     = (uint8_t)type;
    pdu->hdr.seq      = (uint8_t)seq;
    pdu->hdr.ttl      = (uint8_t)ttl;
    pdu->hdr.prio     = (uint8_t)prio;
    pdu->payload_len  = 0u;
}

EXPORT unsigned mb_pdu_get(const void *p, int field)
{
    const manet_pdu_t *pdu = (const manet_pdu_t *)p;
    switch (field) {
    case 0:  return pdu->hdr.src;
    case 1:  return pdu->hdr.dst;
    case 2:  return pdu->hdr.type;
    case 3:  return pdu->hdr.seq;
    case 4:  return pdu->hdr.ttl;
    case 5:  return pdu->hdr.prio;
    case 6:  return pdu->hdr.prev;
    default: return 0u;
    }
}

EXPORT void mb_pdu_copy(void *dst, const void *src)
{
    memcpy(dst, src, sizeof(manet_pdu_t));
}

/* --------------------------------------------------------------- slot.h -- */

EXPORT void mb_sched_init(void *s) { manet_sched_init((manet_sched_t *)s); }

EXPORT int mb_sched_originate(void *s, const void *pdu, unsigned long long slot)
{
    return (int)manet_sched_originate((manet_sched_t *)s, (const manet_pdu_t *)pdu,
                                      (uint64_t)slot);
}

EXPORT int mb_sched_relay(void *s, const void *pdu, unsigned long long rx_slot,
                          unsigned me)
{
    return (int)manet_sched_relay((manet_sched_t *)s, (const manet_pdu_t *)pdu,
                                  (uint64_t)rx_slot, (manet_addr_t)me);
}

EXPORT int mb_sched_take(void *s, unsigned long long slot, void *out)
{
    return manet_sched_take((manet_sched_t *)s, (uint64_t)slot, (manet_pdu_t *)out) ? 1 : 0;
}

EXPORT int mb_sched_suppress(void *s, unsigned src, unsigned seq)
{
    return manet_sched_suppress((manet_sched_t *)s, (manet_addr_t)src, (uint8_t)seq) ? 1 : 0;
}

EXPORT unsigned long mb_sched_depth(const void *s)
{
    return (unsigned long)manet_sched_depth((const manet_sched_t *)s);
}

EXPORT unsigned long long mb_slot_start_us(unsigned long long n)
{
    return manet_slot_start_us((uint64_t)n);
}

/* ---------------------------------------------------------- neighbour.h -- */

EXPORT void mb_nb_init(void *t, unsigned self)
{
    manet_nb_init((manet_nb_table_t *)t, (manet_addr_t)self);
}

EXPORT int mb_nb_heard(void *t, unsigned from, unsigned quality, unsigned long long slot)
{
    return (int)manet_nb_heard((manet_nb_table_t *)t, (manet_addr_t)from,
                               (uint8_t)quality, (uint64_t)slot);
}

/* Parallel byte arrays in, struct array assembled here — Python never builds a C
 * struct, so there is no layout to keep in step. */
EXPORT int mb_nb_advert(void *t, unsigned from, const unsigned char *addrs,
                        const unsigned char *codes, unsigned long n,
                        unsigned long long slot)
{
    manet_advert_t adv[MANET_MAX_NEIGHBOURS];
    unsigned long  i;

    if (n > (unsigned long)MANET_MAX_NEIGHBOURS) {
        n = (unsigned long)MANET_MAX_NEIGHBOURS;
    }
    for (i = 0ul; i < n; i++) {
        adv[i].addr = (manet_addr_t)addrs[i];
        adv[i].code = (manet_adv_code_t)codes[i];
    }
    return (int)manet_nb_advert((manet_nb_table_t *)t, (manet_addr_t)from, adv,
                                (size_t)n, (uint64_t)slot);
}

EXPORT unsigned long mb_nb_expire(void *t, unsigned long long slot)
{
    return (unsigned long)manet_nb_expire((manet_nb_table_t *)t, (uint64_t)slot);
}

EXPORT unsigned long mb_nb_count(const void *t)
{
    return (unsigned long)manet_nb_count((const manet_nb_table_t *)t);
}

EXPORT int mb_nb_link(const void *t, unsigned a)
{
    return (int)manet_nb_link((const manet_nb_table_t *)t, (manet_addr_t)a);
}

EXPORT int mb_nb_should_relay_for(const void *t, unsigned from)
{
    return manet_nb_should_relay_for((const manet_nb_table_t *)t, (manet_addr_t)from) ? 1 : 0;
}

EXPORT unsigned long mb_nb_symmetric(const void *t, unsigned char *out, unsigned long cap)
{
    manet_addr_t  buf[MANET_MAX_NEIGHBOURS];
    size_t        n = manet_nb_symmetric((const manet_nb_table_t *)t, buf,
                                         (size_t)MANET_MAX_NEIGHBOURS);
    unsigned long i;

    for (i = 0ul; i < (unsigned long)n && i < cap; i++) {
        out[i] = (unsigned char)buf[i];
    }
    return (unsigned long)n;
}

EXPORT unsigned long mb_nb_two_hop(const void *t, unsigned char *out, unsigned long cap)
{
    manet_addr_t  buf[MANET_MAX_TWO_HOP];
    size_t        n = manet_nb_two_hop((const manet_nb_table_t *)t, buf,
                                       (size_t)MANET_MAX_TWO_HOP);
    unsigned long i;

    for (i = 0ul; i < (unsigned long)n && i < cap; i++) {
        out[i] = (unsigned char)buf[i];
    }
    return (unsigned long)n;
}

EXPORT unsigned long mb_nb_beacon(const void *t, const unsigned char *mprs,
                                  unsigned long mpr_count,
                                  unsigned char *out_addr, unsigned char *out_code,
                                  unsigned long cap)
{
    manet_addr_t   m[MANET_MAX_NEIGHBOURS];
    manet_advert_t adv[MANET_MAX_NEIGHBOURS];
    unsigned long  i;
    size_t         n;

    if (mpr_count > (unsigned long)MANET_MAX_NEIGHBOURS) {
        mpr_count = (unsigned long)MANET_MAX_NEIGHBOURS;
    }
    for (i = 0ul; i < mpr_count; i++) {
        m[i] = (manet_addr_t)mprs[i];
    }

    n = manet_nb_beacon((const manet_nb_table_t *)t, m, (size_t)mpr_count, adv,
                        (size_t)MANET_MAX_NEIGHBOURS);
    for (i = 0ul; i < (unsigned long)n && i < cap; i++) {
        out_addr[i] = (unsigned char)adv[i].addr;
        out_code[i] = (unsigned char)adv[i].code;
    }
    return (unsigned long)n;
}

/* ---------------------------------------------------------------- mpr.h -- */

EXPORT unsigned long mb_mpr_select(const void *t, void *set, unsigned char *out,
                                   unsigned long cap)
{
    manet_mpr_set_t *s = (manet_mpr_set_t *)set;
    unsigned long    i;

    manet_mpr_select((const manet_nb_table_t *)t, s);
    for (i = 0ul; i < (unsigned long)s->count && i < cap; i++) {
        out[i] = (unsigned char)s->addr[i];
    }
    return (unsigned long)s->count;
}

EXPORT int mb_mpr_covers_all(const void *t, const void *set)
{
    return manet_mpr_covers_all((const manet_nb_table_t *)t,
                                (const manet_mpr_set_t *)set) ? 1 : 0;
}

/* -------------------------------------------------------------- dedup.h -- */

EXPORT void mb_dedup_init(void *d) { manet_dedup_init((manet_dedup_t *)d); }

EXPORT int mb_dedup_check(void *d, unsigned src, unsigned seq, unsigned long long slot)
{
    return manet_dedup_check((manet_dedup_t *)d, (manet_addr_t)src, (uint8_t)seq,
                             (uint64_t)slot) ? 1 : 0;
}

EXPORT unsigned long mb_dedup_expire(void *d, unsigned long long slot,
                                     unsigned long long age)
{
    return (unsigned long)manet_dedup_expire((manet_dedup_t *)d, (uint64_t)slot,
                                             (uint64_t)age);
}

EXPORT int mb_mpr_should_relay(const void *t, unsigned from)
{
    return manet_mpr_should_relay((const manet_nb_table_t *)t, (manet_addr_t)from) ? 1 : 0;
}

/* --------------------------------------------------------------- nama.h -- */

EXPORT unsigned long mb_nama_priority(unsigned node, unsigned long long ctx)
{
    return (unsigned long)manet_nama_priority((manet_addr_t)node, (uint64_t)ctx);
}

EXPORT int mb_nama_wins(const void *t, unsigned long long ctx)
{
    return manet_nama_wins((const manet_nb_table_t *)t, (uint64_t)ctx) ? 1 : 0;
}

EXPORT int mb_nama_next_win(const void *t, unsigned long long from, unsigned limit,
                            unsigned long long *out)
{
    uint64_t got = 0u;
    if (!manet_nama_next_win((const manet_nb_table_t *)t, (uint64_t)from,
                             (uint32_t)limit, &got)) {
        return 0;
    }
    *out = (unsigned long long)got;
    return 1;
}

EXPORT unsigned long mb_nama_contenders(const void *t)
{
    return (unsigned long)manet_nama_contenders((const manet_nb_table_t *)t);
}

EXPORT int mb_slot_is_control(unsigned long long n)
{
    return manet_slot_is_control((uint64_t)n) ? 1 : 0;
}

EXPORT unsigned long long mb_slot_next_voice(unsigned long long n)
{
    return (unsigned long long)manet_slot_next_voice((uint64_t)n);
}
