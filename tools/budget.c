/*
 * budget.c — print the slot budget for one compiled configuration.
 *
 * Not part of the core. Exists so that OQ-0002 can be swept rather than argued:
 * `make budget` builds this once per candidate frame structure and prints the
 * comparison. See docs/open-questions.md#oq-0002.
 */
#include <stdio.h>

#include "manet/config.h"

#ifndef BUDGET_LABEL
#define BUDGET_LABEL "default"
#endif

int main(void)
{
#ifdef BUDGET_BEACON
    /*
     * Beacon overhead — one of Phase 0's five required answers, and the first cut at
     * OQ-0004. Two costs, and the gap between them is the finding.
     */
    printf("  group size                    %ld leaders\n", (long)MANET_TYPICAL_GROUP);
    printf("  beacon interval               %ld frames (%ld ms)\n",
           (long)MANET_BEACON_INTERVAL_FRAMES,
           (long)(MANET_BEACON_INTERVAL_FRAMES * MANET_FRAME_DURATION_US / 1000L));
    printf("  one beacon                    %ld bits (%ld header + %ld count + %ld x %ld entries)\n",
           (long)MANET_BEACON_BITS, (long)MANET_HEADER_BITS, MANET_BEACON_COUNT_BITS,
           (long)(MANET_TYPICAL_GROUP - 1u), MANET_BEACON_ENTRY_BITS);
    printf("  channel per interval          %ld bits in %ld slots\n",
           (long)MANET_INTERVAL_BITS, (long)MANET_INTERVAL_SLOTS);
    printf("\n");
    printf("  what beacons SAY              %ld.%ld%% of channel capacity\n",
           (long)(MANET_BEACON_INFO_PERMILLE / 10L), (long)(MANET_BEACON_INFO_PERMILLE % 10L));
    printf("  what beacons OCCUPY           %ld.%ld%% of slots\n",
           (long)(MANET_BEACON_SLOT_PERMILLE / 10L), (long)(MANET_BEACON_SLOT_PERMILLE % 10L));
    printf("  wasted to packing             %ld bits per beacon\n",
           (long)(MANET_MAX_PDU_BITS - MANET_BEACON_BITS));
    return 0;
#else
    printf("%-24s %6ld %6ld %6ld %6ld %6ld %7ld %8ld%%\n",
           BUDGET_LABEL,
           (long)MANET_SLOT_DURATION_US,
           (long)MANET_SLOT_RAW_BITS,
           (long)MANET_SLOT_ONAIR_BITS,
           (long)MANET_HEADER_BITS,
           (long)MANET_VOICE_PAYLOAD_BITS,
           (long)MANET_FEC_BITS_AVAILABLE,
           (long)MANET_FEC_PERCENT);
    return 0;
#endif
}
