/*
 * trace.c — run a chain of radios through the real scheduler and emit what happened.
 *
 * Not a simulator. There is no propagation model, no collisions, no relay selection —
 * those are neighbour, mpr and the Python harness, none of which exist yet. This drives
 * the actual core/slot scheduler over an idealised chain where each node hears only the
 * one before it, and prints which radio transmits in which slot.
 *
 * The point is to show the pipelining rule doing its job, and to make spatial reuse
 * (OQ-0013) visible: with N slots per frame, the originator and the node N hops away
 * end up transmitting in the same slot at the same instant.
 *
 * Output is JSON on stdout.
 */
#include <stdio.h>
#include <string.h>

#include "manet/slot.h"

#define NODES 10
#define SLOTS 28
#define TTL_START 15

int main(void)
{
    manet_sched_t sched[NODES];
    manet_pdu_t   tx[NODES];
    int           tx_valid[NODES];
    unsigned      k;
    uint64_t      s;
    uint8_t       seq = 0u;
    int           first_row = 1;

    for (k = 0u; k < NODES; k++) {
        manet_sched_init(&sched[k]);
    }

    printf("{\n");
    printf("  \"slots_per_frame\": %ld,\n", (long)MANET_SLOTS_PER_FRAME);
    printf("  \"slot_us\": %ld,\n", (long)MANET_SLOT_DURATION_US);
    printf("  \"frame_us\": %ld,\n", (long)MANET_FRAME_DURATION_US);
    printf("  \"onair_bits\": %ld,\n", (long)MANET_SLOT_ONAIR_BITS);
    printf("  \"fec_bits\": %ld,\n", (long)MANET_FEC_BITS_AVAILABLE);
    printf("  \"fec_percent\": %ld,\n", (long)MANET_FEC_PERCENT);
    printf("  \"nodes\": %d,\n", NODES);
    printf("  \"trace\": [\n");

    for (s = 0u; s < (uint64_t)SLOTS; s++) {
        manet_slot_pos_t pos;
        manet_slot_from_number(s, &pos);

        /* Node 0 is the person holding the PTT. Codec2 produces one payload per frame,
         * so a new burst starts at slot 0 of every frame. */
        if (pos.index == 0u) {
            manet_pdu_t voice;
            memset(&voice, 0, sizeof voice);
            voice.hdr.src  = 0x01u;
            voice.hdr.dst  = 0xC0u;          /* the talkgroup */
            voice.hdr.type = MANET_FRAME_VOICE;
            voice.hdr.seq  = seq++;
            voice.hdr.ttl  = TTL_START;
            voice.hdr.prio = MANET_PRIO_VOICE;
            (void)manet_sched_originate(&sched[0], &voice, s);
        }

        /* Everyone transmits whatever this slot owes. */
        for (k = 0u; k < NODES; k++) {
            tx_valid[k] = manet_sched_take(&sched[k], s, &tx[k]) ? 1 : 0;
        }

        /* Each node hears only its upstream neighbour, and relays in the next slot.
         * The relay decision is hardcoded here — deciding whether to relay is mpr and
         * dedup, which are not written yet. */
        for (k = 1u; k < NODES; k++) {
            if (tx_valid[k - 1u]) {
                (void)manet_sched_relay(&sched[k], &tx[k - 1u], s);
            }
        }

        for (k = 0u; k < NODES; k++) {
            if (!tx_valid[k]) {
                continue;
            }
            printf("%s    {\"slot\": %lu, \"index\": %u, \"us\": %lu, \"node\": %u,"
                   " \"seq\": %u, \"ttl\": %u}",
                   first_row ? "" : ",\n",
                   (unsigned long)s, (unsigned)pos.index,
                   (unsigned long)pos.start_us, k,
                   (unsigned)tx[k].hdr.seq, (unsigned)tx[k].hdr.ttl);
            first_row = 0;
        }
    }

    printf("\n  ]\n}\n");
    return 0;
}
