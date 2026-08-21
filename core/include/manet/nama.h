/*
 * manet/nama.h — Node Activation Multiple Access.
 *
 * Bao & Garcia-Luna-Aceves, "A New Approach to Channel Access Scheduling for Ad Hoc
 * Networks", MobiCom 2001, equations 1-2.
 *
 * Every radio computes, for itself and for every radio within two hops, a pseudo-random
 * priority for the slot in question:
 *
 *     p_k(t) = Rand(k XOR t) XOR k
 *
 * and transmits only if its own is the highest. Because every radio derives the same
 * numbers from the same inputs, the schedule is collision-free BY CONSTRUCTION — no
 * handshake, no reservation, no negotiation, and nothing to go stale. The trailing XOR
 * with the radio's own address makes ties impossible.
 *
 * This replaces hashing an address into a slot pool, which is what this project did
 * before and which has no way to notice a collision. Two radios that hash to the same
 * slot never hear each other and are invisible to every neighbour in common —
 * permanently, and with no error indication. At twelve radios in a 132-slot interval the
 * birthday probability of that is about 39%.
 *
 * The contention set must include TWO-hop neighbours, not just direct ones. A radio
 * cannot hear a collision caused two hops away, and on a relaying mesh that hidden
 * collision lands squarely on the shared relay between them. This is the constraint set
 * every correct distributed slot assignment has used since USAP (MILCOM'96), and it is
 * the reason we maintain a two-hop table at all.
 *
 * Deterministic and integer-only: same answer on x86 and cortex-m4 (ADR-0006).
 */
#ifndef MANET_NAMA_H
#define MANET_NAMA_H

#include <stdbool.h>
#include <stdint.h>

#include "manet/addr.h"
#include "manet/neighbour.h"

/* p_k(t) for one radio in one contention context. Context is normally a slot number. */
uint32_t manet_nama_priority(manet_addr_t node, uint64_t context);

/*
 * True if this radio wins the election for `context` — that is, its priority exceeds
 * every radio within two hops of it.
 *
 * With a two-hop neighbourhood of size D a radio wins roughly one context in D+1, so
 * contention contexts must be drawn from a space large enough to give every radio a turn
 * often enough. That is why slot ownership lives on a superframe above the 60 ms voice
 * frame rather than inside it: an eleven-radio neighbourhood needs at least twelve
 * distinct contexts and a four-slot frame cannot offer them.
 */
bool manet_nama_wins(const manet_nb_table_t *t, uint64_t context);

/* The next context at or after `from` that this radio wins. Bounded by `limit` contexts;
 * returns false if it wins none of them, which means the neighbourhood is larger than the
 * search window and the caller should widen it. */
bool manet_nama_next_win(const manet_nb_table_t *t, uint64_t from, uint32_t limit,
                         uint64_t *out);

/* Size of the contention set — this radio plus everything within two hops. Exposed for
 * sizing the context space and for tests. */
size_t manet_nama_contenders(const manet_nb_table_t *t);

#endif /* MANET_NAMA_H */
