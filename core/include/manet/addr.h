/*
 * manet/addr.h — address space and range predicates.
 *
 * Destination kind is implied by the address range rather than carried in flag bits.
 * See docs/decisions/0007-packet-switched-frame-architecture.md.
 *
 * Pure functions. No state.
 */
#ifndef MANET_ADDR_H
#define MANET_ADDR_H

#include <stdbool.h>
#include <stdint.h>

#include "manet/config.h"

typedef uint8_t manet_addr_t;

typedef enum {
    MANET_ADDR_KIND_NULL = 0,   /* unassigned                                    */
    MANET_ADDR_KIND_HANDHELD,   /* individual mobile node — the normal case      */
    MANET_ADDR_KIND_GATEWAY,    /* individual fixed node with a wired interface  */
    MANET_ADDR_KIND_GROUP,      /* talkgroup — the normal destination for PTT    */
    MANET_ADDR_KIND_RESERVED,   /* future node classes, well-known addresses     */
    MANET_ADDR_KIND_BROADCAST   /* all nodes — beacons, neighbour discovery      */
} manet_addr_kind_t;

manet_addr_kind_t manet_addr_kind(manet_addr_t a);

/* Individual = addresses one specific radio. Handheld or gateway.
 *
 * Note the routing layer must never branch on handheld vs gateway: a gateway runs the
 * identical MAC and routing, and the only difference is what it does with frames
 * addressed to it (Addendum 01 s4). The distinction exists for provisioning, not for
 * protocol behaviour. */
bool manet_addr_is_individual(manet_addr_t a);

/* Multicast = reaches more than one node. Group or broadcast. */
bool manet_addr_is_multicast(manet_addr_t a);

/* A frame must originate from one specific radio. Group, broadcast, reserved and null
 * are never valid sources. */
bool manet_addr_is_valid_source(manet_addr_t a);

/* A frame may be addressed to an individual, a group, or everyone. Null is never a
 * valid destination.
 *
 * Reserved addresses ARE accepted as destinations. A node must be able to relay a frame
 * addressed to a node class it does not yet know about — see manet_header_unpack(). */
bool manet_addr_is_valid_dest(manet_addr_t a);

#endif /* MANET_ADDR_H */
