#include "manet/dedup.h"

void manet_dedup_init(manet_dedup_t *d)
{
    size_t i;
    if (d == NULL) {
        return;
    }
    for (i = 0u; i < (size_t)MANET_DEDUP_DEPTH; i++) {
        d->entries[i].used = false;
    }
    d->next = 0u;
}

bool manet_dedup_seen(const manet_dedup_t *d, manet_addr_t src, uint8_t seq)
{
    size_t i;
    if (d == NULL) {
        return false;
    }
    for (i = 0u; i < (size_t)MANET_DEDUP_DEPTH; i++) {
        if (d->entries[i].used && d->entries[i].src == src && d->entries[i].seq == seq) {
            return true;
        }
    }
    return false;
}

bool manet_dedup_check(manet_dedup_t *d, manet_addr_t src, uint8_t seq, uint64_t slot)
{
    if (d == NULL) {
        return false;
    }
    if (manet_dedup_seen(d, src, seq)) {
        return false;
    }

    /* Oldest entry is overwritten. The ring is sized so that cannot happen while a
     * frame is still in flight — see MANET_DEDUP_DEPTH. */
    d->entries[d->next].src  = src;
    d->entries[d->next].seq  = seq;
    d->entries[d->next].slot = slot;
    d->entries[d->next].used = true;
    d->next = (d->next + 1u) % (size_t)MANET_DEDUP_DEPTH;
    return true;
}

size_t manet_dedup_expire(manet_dedup_t *d, uint64_t slot, uint64_t age)
{
    size_t dropped = 0u;
    size_t i;

    if (d == NULL) {
        return 0u;
    }
    for (i = 0u; i < (size_t)MANET_DEDUP_DEPTH; i++) {
        if (d->entries[i].used && slot > d->entries[i].slot &&
            (slot - d->entries[i].slot) > age) {
            d->entries[i].used = false;
            dropped++;
        }
    }
    return dropped;
}

size_t manet_dedup_count(const manet_dedup_t *d)
{
    size_t n = 0u;
    size_t i;

    if (d == NULL) {
        return 0u;
    }
    for (i = 0u; i < (size_t)MANET_DEDUP_DEPTH; i++) {
        if (d->entries[i].used) {
            n++;
        }
    }
    return n;
}
