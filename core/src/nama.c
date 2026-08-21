#include "manet/nama.h"

#include "manet/mpr.h"

/*
 * Rand(): a deterministic integer mix, not a random number generator. Every radio must
 * compute identical values from identical inputs or the schedule is not collision-free,
 * so this cannot use any source of entropy — and per ADR-0006 it must give the same
 * answer on x86 and on cortex-m4, which rules out anything touching floating point.
 *
 * This is the widely-used 32-bit avalanche mix; it has good bit dispersion, which is all
 * the election needs.
 */
static uint32_t mix32(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

uint32_t manet_nama_priority(manet_addr_t node, uint64_t context)
{
    const uint32_t ctx  = (uint32_t)(context ^ (context >> 32));
    const uint32_t seed = ctx ^ (uint32_t)node;

    /* Rand(k XOR t) XOR k. The trailing XOR is what guarantees uniqueness. */
    return mix32(seed) ^ (uint32_t)node;
}

/* Does `other` outrank `self` for this context? Ties break on address so that two radios
 * can never both conclude they won, even in the vanishingly unlikely case that the mix
 * collides. */
static bool outranks(manet_addr_t other, manet_addr_t self, uint64_t context)
{
    const uint32_t po = manet_nama_priority(other, context);
    const uint32_t ps = manet_nama_priority(self, context);

    if (po != ps) {
        return po > ps;
    }
    return other > self;
}

size_t manet_nama_contenders(const manet_nb_table_t *t)
{
    if (t == NULL) {
        return 0u;
    }
    return 1u + manet_nb_symmetric(t, NULL, 0u) + manet_nb_two_hop(t, NULL, 0u);
}

bool manet_nama_wins(const manet_nb_table_t *t, uint64_t context)
{
    manet_addr_t one[MANET_MAX_NEIGHBOURS];
    manet_addr_t two[MANET_MAX_TWO_HOP];
    size_t       n;
    size_t       i;

    if (t == NULL) {
        return false;
    }

    /* Direct neighbours. A collision with one of these is heard by both, but that is no
     * comfort on a half-duplex radio — neither can hear while transmitting. */
    n = manet_nb_symmetric(t, one, (size_t)MANET_MAX_NEIGHBOURS);
    if (n > (size_t)MANET_MAX_NEIGHBOURS) {
        n = (size_t)MANET_MAX_NEIGHBOURS;
    }
    for (i = 0u; i < n; i++) {
        if (outranks(one[i], t->self, context)) {
            return false;
        }
    }

    /* Two hops away. These are the ones that matter: a radio cannot hear a collision it
     * causes two hops off, and the victim is the relay sitting between them. */
    n = manet_nb_two_hop(t, two, (size_t)MANET_MAX_TWO_HOP);
    if (n > (size_t)MANET_MAX_TWO_HOP) {
        n = (size_t)MANET_MAX_TWO_HOP;
    }
    for (i = 0u; i < n; i++) {
        if (outranks(two[i], t->self, context)) {
            return false;
        }
    }

    /*
     * Net entry. A radio that has heard nobody beats an empty field and would win every
     * single context — so a group switched on together all transmit in every slot,
     * collide, learn nothing about each other, and never converge. Observed directly: a
     * twelve-radio cluster settled at three of twelve reachable.
     *
     * Until the contention set is large enough to be believable, contend against a
     * virtual set of MANET_NAMA_MIN_CONTENDERS instead, by requiring the priority to fall
     * in the top slice of the range. That is slotted-ALOHA entry — the same shape as
     * AIS §3.3.5 and USAP: back off, claim a slot, then settle into the schedule.
     */
    {
        const size_t known = manet_nama_contenders(t);
        if (known < (size_t)MANET_NAMA_MIN_CONTENDERS) {
            const uint32_t share = 0xFFFFFFFFu / (uint32_t)MANET_NAMA_MIN_CONTENDERS;
            if (manet_nama_priority(t->self, context) <
                (0xFFFFFFFFu - share * (uint32_t)known)) {
                return false;
            }
        }
    }

    return true;
}

bool manet_nama_next_win(const manet_nb_table_t *t, uint64_t from, uint32_t limit,
                         uint64_t *out)
{
    uint32_t k;

    if (t == NULL || out == NULL) {
        return false;
    }
    for (k = 0u; k < limit; k++) {
        if (manet_nama_wins(t, from + k)) {
            *out = from + k;
            return true;
        }
    }
    return false;
}
