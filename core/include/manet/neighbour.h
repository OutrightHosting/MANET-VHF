/*
 * manet/neighbour.h — who this radio can hear, and who can hear it back.
 *
 * Maintained from periodic beacons. Each beacon names the neighbours its sender has
 * heard, which does two jobs at once: it proves the sender can hear us (so the link is
 * two-way), and it tells us who is two hops away.
 *
 * The distinction between hearing someone and being heard by them is the important one
 * here. A one-way link is worse than no link — relaying through a radio that cannot hear
 * you fails silently, and looks like a coverage problem. Only symmetric links are ever
 * used for relaying.
 *
 * No allocation, no clock. Time arrives as a slot number (ADR-0006).
 */
#ifndef MANET_NEIGHBOUR_H
#define MANET_NEIGHBOUR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "manet/addr.h"
#include "manet/config.h"
#include "manet/frame.h"

typedef enum {
    MANET_LINK_NONE = 0,
    MANET_LINK_ASYMMETRIC, /* we hear them; no evidence they hear us   */
    MANET_LINK_SYMMETRIC   /* confirmed both ways — usable for relay   */
} manet_link_state_t;

/* Link code carried per entry in a beacon. Two bits on the wire. */
typedef enum {
    MANET_ADV_ASYM = 0, /* sender hears this address, one way so far          */
    MANET_ADV_SYM  = 1, /* sender has a two-way link with this address        */
    MANET_ADV_MPR  = 2  /* sender has selected this address as one of its relays */
} manet_adv_code_t;

typedef struct {
    manet_addr_t     addr;
    manet_adv_code_t code;
} manet_advert_t;

typedef struct {
    manet_addr_t       addr;
    manet_link_state_t link;
    uint8_t            quality;     /* 0 (unusable) .. 255 (perfect)           */
    bool               selected_us; /* they chose us to relay for them         */
    uint64_t           last_heard;  /* slot number                             */
    manet_addr_t       advertised[MANET_MAX_ADVERTISED];
    manet_adv_code_t   adv_code[MANET_MAX_ADVERTISED];
    uint8_t            advertised_count;
} manet_neighbour_t;

typedef struct {
    manet_addr_t      self;
    manet_neighbour_t entries[MANET_MAX_NEIGHBOURS];
    uint8_t           count;
} manet_nb_table_t;

void manet_nb_init(manet_nb_table_t *t, manet_addr_t self);

/*
 * A transmission was received directly from `from`. Establishes or refreshes an
 * asymmetric link — we know we can hear them, and nothing yet about the reverse.
 */
manet_status_t manet_nb_heard(manet_nb_table_t *t, manet_addr_t from,
                              uint8_t quality, uint64_t slot);

/*
 * A beacon from `from` listing the neighbours it has heard.
 *
 * If we appear anywhere in that list — under any link code — they can hear us, and the
 * link becomes symmetric. If we appear with MANET_ADV_MPR, they have selected us to
 * relay on their behalf, which is what gates the relay decision later.
 */
manet_status_t manet_nb_advert(manet_nb_table_t *t, manet_addr_t from,
                               const manet_advert_t *adv, size_t n, uint64_t slot);

/* Drop neighbours not heard within the holding time. Returns how many went. */
size_t manet_nb_expire(manet_nb_table_t *t, uint64_t slot);

/* ------------------------------------------------------------------ queries -- */

size_t                   manet_nb_count(const manet_nb_table_t *t);
const manet_neighbour_t *manet_nb_get(const manet_nb_table_t *t, manet_addr_t a);
manet_link_state_t       manet_nb_link(const manet_nb_table_t *t, manet_addr_t a);

/* Should this radio relay traffic originated by `from`? True only if they selected us. */
bool manet_nb_should_relay_for(const manet_nb_table_t *t, manet_addr_t from);

/* Symmetric one-hop neighbours. */
size_t manet_nb_symmetric(const manet_nb_table_t *t, manet_addr_t *out, size_t cap);

/*
 * Two-hop neighbours: addresses our symmetric neighbours can reach two-way, excluding
 * ourselves and anyone already a one-hop neighbour.
 *
 * When this is empty, everyone we can reach is already in direct range — and no relaying
 * is needed at all. That is the clustered case, and it is why a group standing together
 * behaves exactly like a conventional radio.
 */
size_t manet_nb_two_hop(const manet_nb_table_t *t, manet_addr_t *out, size_t cap);

/* True if `via` has a two-way link to `target`, per what `via` advertised. */
bool manet_nb_reaches(const manet_nb_table_t *t, manet_addr_t via, manet_addr_t target);

/*
 * Build this radio's own beacon contents: every neighbour heard, coded by link state,
 * with our chosen relays marked MPR.
 */
size_t manet_nb_beacon(const manet_nb_table_t *t,
                       const manet_addr_t *mprs, size_t mpr_count,
                       manet_advert_t *out, size_t cap);

#endif /* MANET_NEIGHBOUR_H */
