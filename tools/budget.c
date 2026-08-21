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
}
