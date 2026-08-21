/*
 * manet/mpr.h — multipoint relay selection.
 *
 * Each radio picks the smallest set of its neighbours that between them can reach
 * everyone two hops away. Only those neighbours relay its traffic; everyone else hears
 * it and stays quiet.
 *
 * The property that matters operationally falls straight out of this: when every radio
 * is in direct range of every other, nobody is two hops away, so the set is empty and
 * nothing relays at all. A group standing together behaves exactly like a conventional
 * radio — no relaying, no added latency, no capacity lost. Relaying switches itself on
 * only when the group spreads out, which is when it is worth paying for.
 *
 * Selection is deterministic. Ties break on link quality, then on address, so the same
 * neighbourhood always produces the same set regardless of the order things were heard
 * in. ADR-0006 requires failures to reproduce exactly.
 */
#ifndef MANET_MPR_H
#define MANET_MPR_H

#include <stdbool.h>
#include <stddef.h>

#include "manet/addr.h"
#include "manet/config.h"
#include "manet/neighbour.h"

typedef struct {
    manet_addr_t addr[MANET_MAX_NEIGHBOURS];
    size_t       count;
} manet_mpr_set_t;

/*
 * Choose relays. An empty result means no relaying is needed — see above.
 *
 * Follows OLSR's heuristic: neighbours that are the only route to someone are mandatory,
 * then greedily take whichever covers the most of what is still unreachable, then drop
 * any that turn out to be redundant.
 */
void manet_mpr_select(const manet_nb_table_t *t, manet_mpr_set_t *out);

bool manet_mpr_contains(const manet_mpr_set_t *s, manet_addr_t a);

/*
 * Should this radio relay a frame it just received from `from`?
 *
 * True if this radio can reach at least one two-way neighbour that `from` cannot. If it
 * reaches nobody new, relaying adds nothing and it stays quiet.
 *
 * This is decided entirely from what this radio already knows — its own neighbours, and
 * the neighbour list `from` last advertised. It does NOT require `from` to have named
 * this radio as a relay.
 *
 * That difference is what makes the network work while people are moving. The selected-
 * relay flag can only arrive in a beacon, and beacons are precisely what cannot get
 * through while a transmission is in progress; a radio that has not heard the beacon
 * naming it simply does not relay, and the chain dies. This rule degrades far more
 * gracefully: stale knowledge makes a radio relay when it needn't, which duplicate
 * suppression and passive acknowledgement already handle, rather than not relay when it
 * must, which nothing recovers from.
 *
 * The clustered case still costs nothing. When every radio is in direct range of every
 * other, nobody reaches anyone the sender does not, so nothing relays.
 */
bool manet_mpr_should_relay(const manet_nb_table_t *t, manet_addr_t from);

/* True if every two-hop neighbour is reachable through the set. Used by the redundancy
 * pass, and worth asserting in tests: a set that fails this is a coverage hole. */
bool manet_mpr_covers_all(const manet_nb_table_t *t, const manet_mpr_set_t *s);

#endif /* MANET_MPR_H */
