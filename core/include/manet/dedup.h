/*
 * manet/dedup.h — have I heard this frame before?
 *
 * On a mesh every frame arrives repeatedly: from the originator, and again from each
 * relay that carries it. Without suppression a single transmission would echo around
 * the group until its TTL ran out, and on a channel this narrow that is fatal rather
 * than merely wasteful.
 *
 * A frame is identified by its origin and sequence number — never by whoever last
 * relayed it, which changes at every hop.
 *
 * Fixed-size ring. No allocation, no clock (ADR-0006).
 */
#ifndef MANET_DEDUP_H
#define MANET_DEDUP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "manet/addr.h"
#include "manet/config.h"

typedef struct {
    manet_addr_t src;
    uint8_t      seq;
    bool         used;
    uint64_t     slot;
} manet_dedup_entry_t;

typedef struct {
    manet_dedup_entry_t entries[MANET_DEDUP_DEPTH];
    size_t              next;
} manet_dedup_t;

void manet_dedup_init(manet_dedup_t *d);

/*
 * True if this frame has not been seen before — in which case it is recorded, and the
 * caller should process and possibly relay it. False means it is an echo: drop it.
 *
 * Note the deliberate side effect. Asking is recording. A caller that wants to test
 * without consuming should use manet_dedup_seen().
 */
bool manet_dedup_check(manet_dedup_t *d, manet_addr_t src, uint8_t seq, uint64_t slot);

/* Test without recording. */
bool manet_dedup_seen(const manet_dedup_t *d, manet_addr_t src, uint8_t seq);

/* Forget entries older than `age` slots. Sequence numbers are only 8 bits and wrap
 * (OQ-0012), so stale entries must go or a wrapped sequence looks like an echo. */
size_t manet_dedup_expire(manet_dedup_t *d, uint64_t slot, uint64_t age);

size_t manet_dedup_count(const manet_dedup_t *d);

#endif /* MANET_DEDUP_H */
