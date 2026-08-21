/*
 * bits.h — internal MSB-first bit reader/writer.
 *
 * Not part of the public API. `frame` is the only user.
 *
 * Bit-at-a-time, which is slow and deliberate: this packs a ~34-bit header once per
 * 60 ms frame, so clarity is worth more than speed, and the loop is obviously correct
 * on both x86 and ARM. ADR-0006 requires the two to agree bit for bit.
 *
 * The writer sets and clears explicitly, so the destination buffer need not be zeroed.
 */
#ifndef MANET_BITS_H
#define MANET_BITS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t *buf;
    size_t   cap_bits;
    size_t   pos_bits;
} manet_bitw_t;

typedef struct {
    const uint8_t *buf;
    size_t         cap_bits;
    size_t         pos_bits;
} manet_bitr_t;

static inline void manet_bitw_init(manet_bitw_t *w, uint8_t *buf, size_t cap_bytes)
{
    w->buf      = buf;
    w->cap_bits = cap_bytes * 8u;
    w->pos_bits = 0u;
}

static inline void manet_bitr_init(manet_bitr_t *r, const uint8_t *buf, size_t cap_bytes)
{
    r->buf      = buf;
    r->cap_bits = cap_bytes * 8u;
    r->pos_bits = 0u;
}

static inline bool manet_bitw_put(manet_bitw_t *w, uint32_t v, unsigned n)
{
    unsigned i;

    if (n == 0u || n > 32u) {
        return false;
    }
    if (w->pos_bits + n > w->cap_bits) {
        return false;
    }
    /* Reject values that would be silently truncated. A caller passing a field wider
     * than its declared width has a bug, and it must not reach the air. */
    if (n < 32u && (v >> n) != 0u) {
        return false;
    }

    for (i = 0u; i < n; i++) {
        const unsigned bit   = (unsigned)((v >> (n - 1u - i)) & 1u);
        const size_t   p     = w->pos_bits + i;
        const size_t   byte  = p >> 3;
        const unsigned shift = 7u - (unsigned)(p & 7u);

        if (bit != 0u) {
            w->buf[byte] |= (uint8_t)(1u << shift);
        } else {
            w->buf[byte] = (uint8_t)(w->buf[byte] & (uint8_t)~(1u << shift));
        }
    }
    w->pos_bits += n;
    return true;
}

static inline bool manet_bitr_get(manet_bitr_t *r, uint32_t *out, unsigned n)
{
    uint32_t v = 0u;
    unsigned i;

    if (out == NULL || n == 0u || n > 32u) {
        return false;
    }
    if (r->pos_bits + n > r->cap_bits) {
        return false;
    }

    for (i = 0u; i < n; i++) {
        const size_t   p     = r->pos_bits + i;
        const size_t   byte  = p >> 3;
        const unsigned shift = 7u - (unsigned)(p & 7u);
        const unsigned bit   = (unsigned)((r->buf[byte] >> shift) & 1u);

        v = (v << 1) | (uint32_t)bit;
    }
    r->pos_bits += n;
    *out = v;
    return true;
}

#endif /* MANET_BITS_H */
