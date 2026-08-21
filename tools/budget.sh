#!/bin/sh
#
# Sweep the slot budget across candidate frame structures.
#
# OQ-0002 says the budget does not close. This is how that gets checked rather than
# argued: each row is a real compilation of core/include/manet/config.h.
#
# MANET_ALLOW_INFEASIBLE is defined so that configurations which do not fit report a
# negative FEC budget instead of failing to build. Only this tool may define it.
set -e

CC=${CC:-cc}
OUT=${1:-build}
CFLAGS="-std=c99 -Wall -Wextra -pedantic -Icore/include -DMANET_ALLOW_INFEASIBLE"

mkdir -p "$OUT"

row() {
    label="$1"
    shift
    # shellcheck disable=SC2086
    $CC $CFLAGS -DBUDGET_LABEL="\"$label\"" "$@" tools/budget.c -o "$OUT/budget"
    "$OUT/budget"
}

printf '%-24s %6s %6s %6s %6s %6s %7s %9s\n' \
    'configuration' 'slot' 'raw' 'onair' 'header' 'voice' 'FEC' 'FEC ratio'
printf '%-24s %6s %6s %6s %6s %6s %7s %9s\n' \
    '------------------------' '----' '----' '-----' '------' '-----' '------' '---------'

# Slot count cannot be varied alone: config.h asserts the frame divides evenly into
# slots, and 160 ms does not divide by 3. Every 3-slot row therefore carries a frame
# duration that does. 159 ms is the nearest to the real 160 ms frame.
THREE='-DMANET_SLOTS_PER_FRAME=3L -DMANET_FRAME_DURATION_US=159000L'

# shellcheck disable=SC2086
row '4x40ms 19.2k ADR-0009'
# shellcheck disable=SC2086
row '3x53ms 19.2k'           $THREE
row '2x80ms 19.2k'           -DMANET_SLOTS_PER_FRAME=2L
row '4x40ms 22.4k'           -DMANET_GROSS_BITRATE_BPS=22400L
row '4x40ms 16.0k'           -DMANET_GROSS_BITRATE_BPS=16000L
# shellcheck disable=SC2086
row '3x53ms 16.0k'           $THREE -DMANET_GROSS_BITRATE_BPS=16000L
# Codec2 2400 over a 160 ms frame is 384 bits, not 144 — that figure was the 60 ms frame.
row '4x40ms 19.2k codec2400' -DMANET_VOICE_PAYLOAD_BITS=384L
row '4x40ms 19.2k seq 6b'    -DMANET_SEQ_BITS=6u

echo
echo 'Beacon overhead at the default interval — OQ-0004, first cut:'
echo
$CC $CFLAGS -DBUDGET_BEACON tools/budget.c -o "$OUT/beacon4"
"$OUT/beacon4"
echo
echo '  (the same, at 3 x 53 ms)'
# shellcheck disable=SC2086
$CC $CFLAGS -DBUDGET_BEACON $THREE tools/budget.c -o "$OUT/beacon3"
"$OUT/beacon3"

echo
echo 'Notes:'
echo '  FEC ratio is FEC bits as a percentage of what they protect (header + voice).'
echo '  DMR runs ~47%. Below ~25% is unlikely to be viable on a channel with no'
echo '  retransmission — see OQ-0002.'
echo '  2x30ms is listed for reference only: two slots gives a spatial reuse distance'
echo '  of 2, which is exactly the hidden-terminal case (OQ-0013).'
echo '  The 120 ms superframe option is not modelled here — it spans one voice payload'
echo '  across two slot opportunities, which this one-payload-per-slot model cannot'
echo '  express. It remains a live escape route in OQ-0002.'
echo
echo '  Header width is not an escape route. Address width cannot be swept until the'
echo '  address map is re-derived for a narrower space (config.h asserts this), so the'
echo '  only trimmable field is the sequence number: 8->6 bits recovers 2 bits of 264.'
echo '  More decisively, at 4x15ms/19.2k/Codec2-3200 a ZERO-length header would still'
echo '  leave only 264-24-192 = 48 bits, a 25% ratio against DMR\047s 47%. The 4-slot'
echo '  configuration cannot reach DMR-equivalent protection under any header design.'
echo
echo '  On beacons: the gap between what they SAY and what they OCCUPY is packing loss.'
echo '  A beacon is far smaller than a slot, and a slot is the smallest thing that can'
echo '  be transmitted, so most of it is wasted. Closing that gap means packing several'
echo '  beacons per slot or piggybacking them on voice. Neither is designed. OQ-0004.'
