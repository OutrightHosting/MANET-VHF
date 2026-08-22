#include "manet/neighbour.h"

static manet_neighbour_t *find(manet_nb_table_t *t, manet_addr_t a)
{
    uint8_t i;
    for (i = 0u; i < t->count; i++) {
        if (t->entries[i].addr == a) {
            return &t->entries[i];
        }
    }
    return NULL;
}

/*
 * When the table is full, which entry is least worth keeping -- or none.
 *
 * Strict order of preference, and the last clause matters as much as the first:
 *
 *   1. An expired entry. Not heard within the hold time, so probably walked away.
 *      Always evictable, including one that had selected us as its relay: it cannot be
 *      relying on us if it is not there.
 *   2. An asymmetric entry weaker than the newcomer by more than the margin. We can hear
 *      them and have no evidence they can hear us, which is worth less than a confirmed
 *      link -- but it still needs the margin, or two unconfirmed neighbours a hair apart
 *      displace each other for as long as both can be heard.
 *   3. A symmetric entry weaker than the newcomer by more than the margin -- EXCEPT one
 *      that has selected us as its multipoint relay. Dropping that silently breaks
 *      somebody else's forwarding path, and they have no way to discover we did it.
 *   4. Nothing. The newcomer genuinely is not better than what we hold, so refusing is
 *      the correct answer and prevents two similar neighbours displacing each other for
 *      as long as they can both be heard.
 */
static manet_neighbour_t *evict_candidate(manet_nb_table_t *t, uint8_t quality,
                                          uint64_t slot)
{
    manet_neighbour_t *worst = NULL;
    size_t i;

    /* 1. expired -- take the stalest */
    for (i = 0u; i < (size_t)t->count; i++) {
        manet_neighbour_t *e = &t->entries[i];
        if (slot > e->last_heard && (slot - e->last_heard) > MANET_NB_HOLD_SLOTS) {
            if (worst == NULL || e->last_heard < worst->last_heard) {
                worst = e;
            }
        }
    }
    if (worst != NULL) {
        return worst;
    }

    /* Below here the newcomer is competing with something that is genuinely present, so
     * it must clear the margin. The margin applies to BOTH remaining clauses: an
     * asymmetric entry is worth less than a symmetric one, but two asymmetric entries a
     * hair apart in quality will displace each other just as readily as two symmetric
     * ones, and the first version of this function let them. */
    if (quality <= (uint8_t)MANET_NB_EVICT_MARGIN) {
        return NULL;
    }
    {
        uint8_t threshold = (uint8_t)(quality - (uint8_t)MANET_NB_EVICT_MARGIN);

        /* 2. asymmetric and clearly weaker -- take the weakest. Preferred over any
         * symmetric entry: we can hear them and have no evidence they can hear us. */
        for (i = 0u; i < (size_t)t->count; i++) {
            manet_neighbour_t *e = &t->entries[i];
            if (e->link != MANET_LINK_SYMMETRIC && e->quality < threshold) {
                if (worst == NULL || e->quality < worst->quality) {
                    worst = e;
                }
            }
        }
        if (worst != NULL) {
            return worst;
        }

        /* 3. symmetric, not relying on us, and clearly weaker */
        for (i = 0u; i < (size_t)t->count; i++) {
            manet_neighbour_t *e = &t->entries[i];
            if (!e->selected_us && e->quality < threshold) {
                if (worst == NULL || e->quality < worst->quality) {
                    worst = e;
                }
            }
        }
    }
    return worst;   /* 4. NULL means keep what we have */
}

static manet_neighbour_t *find_or_add(manet_nb_table_t *t, manet_addr_t a,
                                      uint8_t quality, uint64_t slot)
{
    manet_neighbour_t *e = find(t, a);
    if (e != NULL) {
        return e;
    }
    if (t->count >= (uint8_t)MANET_MAX_NEIGHBOURS) {
        e = evict_candidate(t, quality, slot);
        if (e == NULL) {
            return NULL;
        }
        /* Reuse the slot in place. The caller initialises the rest below. */
        e->addr             = a;
        e->link             = MANET_LINK_NONE;
        e->quality          = 0u;
        e->selected_us      = false;
        e->last_heard       = 0u;
        e->advertised_count = 0u;
        return e;
    }
    e = &t->entries[t->count];
    t->count++;

    e->addr             = a;
    e->link             = MANET_LINK_NONE;
    e->quality          = 0u;
    e->selected_us      = false;
    e->last_heard       = 0u;
    e->advertised_count = 0u;
    return e;
}

static bool contains(const manet_addr_t *list, size_t n, manet_addr_t a)
{
    size_t i;
    for (i = 0u; i < n; i++) {
        if (list[i] == a) {
            return true;
        }
    }
    return false;
}

void manet_nb_init(manet_nb_table_t *t, manet_addr_t self)
{
    if (t == NULL) {
        return;
    }
    t->self  = self;
    t->count = 0u;
}

manet_status_t manet_nb_heard(manet_nb_table_t *t, manet_addr_t from,
                              uint8_t quality, uint64_t slot)
{
    manet_neighbour_t *e;

    if (t == NULL) {
        return MANET_ERR_NULL_ARG;
    }
    /* A radio is not its own neighbour, and only an individual address can originate. */
    if (from == t->self || !manet_addr_is_individual(from)) {
        return MANET_ERR_BAD_SOURCE;
    }

    e = find_or_add(t, from, quality, slot);
    if (e == NULL) {
        return MANET_ERR_BUFFER;
    }

    /* Hearing them proves one direction only. Symmetry is established in
     * manet_nb_advert, when they name us in a beacon of their own. */
    if (e->link == MANET_LINK_NONE) {
        e->link = MANET_LINK_ASYMMETRIC;
    }
    e->quality    = quality;
    e->last_heard = slot;
    return MANET_OK;
}

manet_status_t manet_nb_advert(manet_nb_table_t *t, manet_addr_t from,
                               const manet_advert_t *adv, size_t n, uint64_t slot)
{
    manet_neighbour_t *e;
    size_t             i;
    uint8_t            kept = 0u;

    if (t == NULL || (adv == NULL && n > 0u)) {
        return MANET_ERR_NULL_ARG;
    }
    if (from == t->self || !manet_addr_is_individual(from)) {
        return MANET_ERR_BAD_SOURCE;
    }

    /* Quality 0: an advert carries no link measurement of its own, so this can only
     * reclaim an EXPIRED entry and can never displace a live neighbour on a number we
     * would have had to invent. In practice manet_nb_heard has already run for the same
     * frame and the entry exists; this is the defensive path. */
    e = find_or_add(t, from, 0u, slot);
    if (e == NULL) {
        return MANET_ERR_BUFFER;
    }
    e->last_heard  = slot;
    e->selected_us = false;

    for (i = 0u; i < n; i++) {
        if (adv[i].addr == t->self) {
            /* They named us, so they can hear us. Any code will do — the mere mention
             * is the proof. This is the moment a one-way link becomes usable. */
            e->link = MANET_LINK_SYMMETRIC;
            if (adv[i].code == MANET_ADV_MPR) {
                e->selected_us = true;
            }
            continue;
        }
        if (kept < (uint8_t)MANET_MAX_ADVERTISED) {
            e->advertised[kept] = adv[i].addr;
            e->adv_code[kept]   = adv[i].code;
            kept++;
        }
    }
    e->advertised_count = kept;
    return MANET_OK;
}

size_t manet_nb_expire(manet_nb_table_t *t, uint64_t slot)
{
    size_t  dropped = 0u;
    uint8_t i       = 0u;

    if (t == NULL) {
        return 0u;
    }

    while (i < t->count) {
        if (slot > t->entries[i].last_heard &&
            (slot - t->entries[i].last_heard) > MANET_NB_HOLD_SLOTS) {
            /* Compact by moving the last entry into the hole. Order is not meaningful. */
            t->count--;
            if (i != t->count) {
                t->entries[i] = t->entries[t->count];
            }
            dropped++;
        } else {
            i++;
        }
    }
    return dropped;
}

size_t manet_nb_count(const manet_nb_table_t *t)
{
    return (t == NULL) ? 0u : (size_t)t->count;
}

const manet_neighbour_t *manet_nb_get(const manet_nb_table_t *t, manet_addr_t a)
{
    uint8_t i;
    if (t == NULL) {
        return NULL;
    }
    for (i = 0u; i < t->count; i++) {
        if (t->entries[i].addr == a) {
            return &t->entries[i];
        }
    }
    return NULL;
}

manet_link_state_t manet_nb_link(const manet_nb_table_t *t, manet_addr_t a)
{
    const manet_neighbour_t *e = manet_nb_get(t, a);
    return (e == NULL) ? MANET_LINK_NONE : e->link;
}

bool manet_nb_should_relay_for(const manet_nb_table_t *t, manet_addr_t from)
{
    const manet_neighbour_t *e = manet_nb_get(t, from);
    /* Relay only for a neighbour that both hears us and has asked us to. */
    return e != NULL && e->link == MANET_LINK_SYMMETRIC && e->selected_us;
}

size_t manet_nb_symmetric(const manet_nb_table_t *t, manet_addr_t *out, size_t cap)
{
    size_t  n = 0u;
    uint8_t i;

    if (t == NULL) {
        return 0u;
    }
    for (i = 0u; i < t->count; i++) {
        if (t->entries[i].link != MANET_LINK_SYMMETRIC) {
            continue;
        }
        if (out != NULL && n < cap) {
            out[n] = t->entries[i].addr;
        }
        n++;
    }
    return n;
}

bool manet_nb_reaches(const manet_nb_table_t *t, manet_addr_t via, manet_addr_t target)
{
    const manet_neighbour_t *e = manet_nb_get(t, via);
    uint8_t                  i;

    if (e == NULL || e->link != MANET_LINK_SYMMETRIC) {
        return false;
    }
    for (i = 0u; i < e->advertised_count; i++) {
        /* An asymmetric link at the far end is no use for relaying either. */
        if (e->advertised[i] == target && e->adv_code[i] != MANET_ADV_ASYM) {
            return true;
        }
    }
    return false;
}

size_t manet_nb_two_hop(const manet_nb_table_t *t, manet_addr_t *out, size_t cap)
{
    manet_addr_t seen[MANET_MAX_TWO_HOP];
    size_t       n = 0u;
    uint8_t      i;
    uint8_t      j;

    if (t == NULL) {
        return 0u;
    }

    for (i = 0u; i < t->count; i++) {
        const manet_neighbour_t *e = &t->entries[i];

        if (e->link != MANET_LINK_SYMMETRIC) {
            continue;
        }
        for (j = 0u; j < e->advertised_count; j++) {
            const manet_addr_t a = e->advertised[j];

            if (e->adv_code[j] == MANET_ADV_ASYM) {
                continue;
            }
            if (a == t->self) {
                continue;
            }
            /* Already in direct range, so not two hops away. */
            if (manet_nb_link(t, a) == MANET_LINK_SYMMETRIC) {
                continue;
            }
            if (contains(seen, n, a)) {
                continue;
            }
            if (n >= (size_t)MANET_MAX_TWO_HOP) {
                continue;
            }
            seen[n] = a;
            if (out != NULL && n < cap) {
                out[n] = a;
            }
            n++;
        }
    }
    return n;
}

size_t manet_nb_beacon(const manet_nb_table_t *t,
                       const manet_addr_t *mprs, size_t mpr_count,
                       manet_advert_t *out, size_t cap)
{
    size_t  n = 0u;
    uint8_t i;

    if (t == NULL) {
        return 0u;
    }
    for (i = 0u; i < t->count; i++) {
        const manet_neighbour_t *e = &t->entries[i];
        manet_adv_code_t         code;

        if (e->link == MANET_LINK_NONE) {
            continue;
        }
        if (e->link == MANET_LINK_ASYMMETRIC) {
            code = MANET_ADV_ASYM;
        } else if (mprs != NULL && contains(mprs, mpr_count, e->addr)) {
            code = MANET_ADV_MPR;
        } else {
            code = MANET_ADV_SYM;
        }

        if (out != NULL && n < cap) {
            out[n].addr = e->addr;
            out[n].code = code;
        }
        n++;
    }
    return n;
}
