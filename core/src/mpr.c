#include "manet/mpr.h"

static bool set_add(manet_mpr_set_t *s, manet_addr_t a)
{
    size_t i;
    for (i = 0u; i < s->count; i++) {
        if (s->addr[i] == a) {
            return true;
        }
    }
    if (s->count >= (size_t)MANET_MAX_NEIGHBOURS) {
        return false;
    }
    s->addr[s->count] = a;
    s->count++;
    return true;
}

static void set_remove_at(manet_mpr_set_t *s, size_t idx)
{
    size_t i;
    for (i = idx + 1u; i < s->count; i++) {
        s->addr[i - 1u] = s->addr[i];
    }
    s->count--;
}

bool manet_mpr_contains(const manet_mpr_set_t *s, manet_addr_t a)
{
    size_t i;
    if (s == NULL) {
        return false;
    }
    for (i = 0u; i < s->count; i++) {
        if (s->addr[i] == a) {
            return true;
        }
    }
    return false;
}

/* How many still-unreached two-hop neighbours this candidate would pick up. */
static size_t gain(const manet_nb_table_t *t, manet_addr_t via,
                   const manet_addr_t *n2, size_t n2c, const bool *covered)
{
    size_t n = 0u;
    size_t j;
    for (j = 0u; j < n2c; j++) {
        if (!covered[j] && manet_nb_reaches(t, via, n2[j])) {
            n++;
        }
    }
    return n;
}

static void mark(const manet_nb_table_t *t, manet_addr_t via,
                 const manet_addr_t *n2, size_t n2c, bool *covered)
{
    size_t j;
    for (j = 0u; j < n2c; j++) {
        if (manet_nb_reaches(t, via, n2[j])) {
            covered[j] = true;
        }
    }
}

static uint8_t quality_of(const manet_nb_table_t *t, manet_addr_t a)
{
    const manet_neighbour_t *e = manet_nb_get(t, a);
    return (e == NULL) ? 0u : e->quality;
}

void manet_mpr_select(const manet_nb_table_t *t, manet_mpr_set_t *out)
{
    manet_addr_t n1[MANET_MAX_NEIGHBOURS];
    manet_addr_t n2[MANET_MAX_TWO_HOP];
    bool         covered[MANET_MAX_TWO_HOP];
    size_t       n1c;
    size_t       n2c;
    size_t       i;
    size_t       j;

    if (out == NULL) {
        return;
    }
    out->count = 0u;
    if (t == NULL) {
        return;
    }

    n1c = manet_nb_symmetric(t, n1, (size_t)MANET_MAX_NEIGHBOURS);
    if (n1c > (size_t)MANET_MAX_NEIGHBOURS) {
        n1c = (size_t)MANET_MAX_NEIGHBOURS;
    }
    n2c = manet_nb_two_hop(t, n2, (size_t)MANET_MAX_TWO_HOP);
    if (n2c > (size_t)MANET_MAX_TWO_HOP) {
        n2c = (size_t)MANET_MAX_TWO_HOP;
    }

    /* Nobody is two hops away. Everyone we can reach, we can reach directly — so no
     * relaying is needed and the set stays empty. This is the clustered group, and it
     * is the single most important behaviour in the product. */
    if (n2c == 0u || n1c == 0u) {
        return;
    }

    for (j = 0u; j < n2c; j++) {
        covered[j] = false;
    }

    /* Anyone who is the sole route to someone is not optional. */
    for (j = 0u; j < n2c; j++) {
        size_t reach = 0u;
        size_t only  = 0u;

        for (i = 0u; i < n1c; i++) {
            if (manet_nb_reaches(t, n1[i], n2[j])) {
                reach++;
                only = i;
            }
        }
        if (reach == 1u) {
            (void)set_add(out, n1[only]);
        }
    }
    for (i = 0u; i < out->count; i++) {
        mark(t, out->addr[i], n2, n2c, covered);
    }

    /* Then take whoever covers the most of what is left. Ties break on link quality and
     * then on address, so the result never depends on the order neighbours were heard
     * in — the same neighbourhood always yields the same set. */
    for (;;) {
        size_t       best_gain = 0u;
        manet_addr_t best      = 0u;
        bool         found     = false;

        for (i = 0u; i < n1c; i++) {
            size_t g;

            if (manet_mpr_contains(out, n1[i])) {
                continue;
            }
            g = gain(t, n1[i], n2, n2c, covered);
            if (g == 0u) {
                continue;
            }
            if (!found || g > best_gain ||
                (g == best_gain &&
                 (quality_of(t, n1[i]) > quality_of(t, best) ||
                  (quality_of(t, n1[i]) == quality_of(t, best) && n1[i] < best)))) {
                best_gain = g;
                best      = n1[i];
                found     = true;
            }
        }
        if (!found) {
            /* Whatever is left is unreachable through any neighbour — a hole in the
             * two-hop view, not something more relays can fix. */
            break;
        }
        if (!set_add(out, best)) {
            break;
        }
        mark(t, best, n2, n2c, covered);
    }

    /* The mandatory and greedy passes can between them pick someone who turns out to
     * add nothing. Drop them: every extra relay is a retransmission on a channel that
     * cannot spare one. */
    i = out->count;
    while (i > 0u) {
        manet_mpr_set_t trial = *out;

        i--;
        set_remove_at(&trial, i);
        if (manet_mpr_covers_all(t, &trial)) {
            *out = trial;
        }
    }
}

bool manet_mpr_covers_all(const manet_nb_table_t *t, const manet_mpr_set_t *s)
{
    manet_addr_t n2[MANET_MAX_TWO_HOP];
    size_t       n2c;
    size_t       i;
    size_t       j;

    if (t == NULL || s == NULL) {
        return false;
    }
    n2c = manet_nb_two_hop(t, n2, (size_t)MANET_MAX_TWO_HOP);
    if (n2c > (size_t)MANET_MAX_TWO_HOP) {
        n2c = (size_t)MANET_MAX_TWO_HOP;
    }

    for (j = 0u; j < n2c; j++) {
        bool ok = false;
        for (i = 0u; i < s->count; i++) {
            if (manet_nb_reaches(t, s->addr[i], n2[j])) {
                ok = true;
                break;
            }
        }
        if (!ok) {
            /* Unreachable through anyone at all is a hole in the neighbourhood, not a
             * failure of this set — do not report it as one. */
            bool reachable_at_all = false;
            manet_addr_t n1[MANET_MAX_NEIGHBOURS];
            size_t n1c = manet_nb_symmetric(t, n1, (size_t)MANET_MAX_NEIGHBOURS);
            if (n1c > (size_t)MANET_MAX_NEIGHBOURS) {
                n1c = (size_t)MANET_MAX_NEIGHBOURS;
            }
            for (i = 0u; i < n1c; i++) {
                if (manet_nb_reaches(t, n1[i], n2[j])) {
                    reachable_at_all = true;
                    break;
                }
            }
            if (reachable_at_all) {
                return false;
            }
        }
    }
    return true;
}
