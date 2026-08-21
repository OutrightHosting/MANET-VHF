#include "manet/frame.h"

#include "bits.h"

manet_status_t manet_header_pack(const manet_header_t *h, uint8_t *buf, size_t cap_bytes)
{
    manet_bitw_t w;

    if (h == NULL || buf == NULL) {
        return MANET_ERR_NULL_ARG;
    }
    if (cap_bytes < MANET_HEADER_BYTES) {
        return MANET_ERR_BUFFER;
    }

    manet_bitw_init(&w, buf, cap_bytes);

    /* manet_bitw_put rejects a value too wide for its field, so a field-range bug
     * cannot reach the air. Wire order is fixed: src, dst, type, seq, ttl, prio. */
    if (!manet_bitw_put(&w, (uint32_t)h->src,  MANET_ADDR_BITS) ||
        !manet_bitw_put(&w, (uint32_t)h->dst,  MANET_ADDR_BITS) ||
        !manet_bitw_put(&w, (uint32_t)h->type, MANET_TYPE_BITS) ||
        !manet_bitw_put(&w, (uint32_t)h->seq,  MANET_SEQ_BITS)  ||
        !manet_bitw_put(&w, (uint32_t)h->ttl,  MANET_TTL_BITS)  ||
        !manet_bitw_put(&w, (uint32_t)h->prio, MANET_PRIO_BITS)) {
        return MANET_ERR_FIELD_RANGE;
    }

    return MANET_OK;
}

manet_status_t manet_header_unpack(manet_header_t *h, const uint8_t *buf, size_t cap_bytes)
{
    manet_bitr_t r;
    uint32_t     src, dst, type, seq, ttl, prio;

    if (h == NULL || buf == NULL) {
        return MANET_ERR_NULL_ARG;
    }
    if (cap_bytes < MANET_HEADER_BYTES) {
        return MANET_ERR_BUFFER;
    }

    manet_bitr_init(&r, buf, cap_bytes);

    if (!manet_bitr_get(&r, &src,  MANET_ADDR_BITS) ||
        !manet_bitr_get(&r, &dst,  MANET_ADDR_BITS) ||
        !manet_bitr_get(&r, &type, MANET_TYPE_BITS) ||
        !manet_bitr_get(&r, &seq,  MANET_SEQ_BITS)  ||
        !manet_bitr_get(&r, &ttl,  MANET_TTL_BITS)  ||
        !manet_bitr_get(&r, &prio, MANET_PRIO_BITS)) {
        return MANET_ERR_BUFFER;
    }

    h->src  = (manet_addr_t)src;
    h->dst  = (manet_addr_t)dst;
    h->type = (uint8_t)type;
    h->seq  = (uint8_t)seq;
    h->ttl  = (uint8_t)ttl;
    h->prio = (uint8_t)prio;

    /* Deliberately no semantic rejection here. An unrecognised type or a reserved
     * destination must still parse, so that a node can relay traffic for a network
     * newer than itself. See the note in manet/frame.h. */
    return MANET_OK;
}

manet_status_t manet_header_validate(const manet_header_t *h)
{
    if (h == NULL) {
        return MANET_ERR_NULL_ARG;
    }
    if (!manet_addr_is_valid_source(h->src)) {
        return MANET_ERR_BAD_SOURCE;
    }
    if (!manet_addr_is_valid_dest(h->dst)) {
        return MANET_ERR_BAD_DEST;
    }
    if (!manet_frame_type_is_known(h->type)) {
        return MANET_ERR_RESERVED_TYPE;
    }
    if (h->ttl == 0u) {
        return MANET_ERR_TTL_EXPIRED;
    }
    if (h->ttl > MANET_TTL_MAX) {
        return MANET_ERR_FIELD_RANGE;
    }
    if (h->prio > (uint8_t)MANET_PRIO_DATA) {
        return MANET_ERR_FIELD_RANGE;
    }
    return MANET_OK;
}

bool manet_frame_type_is_known(uint8_t type)
{
    return type <= (uint8_t)MANET_FRAME_TYPE_MAX;
}

manet_priority_t manet_frame_default_priority(uint8_t type)
{
    switch (type) {
    case MANET_FRAME_EMERGENCY:
        return MANET_PRIO_EMERGENCY;
    case MANET_FRAME_VOICE:
    case MANET_FRAME_VOICE_END:
        return MANET_PRIO_VOICE;
    case MANET_FRAME_BEACON:
    case MANET_FRAME_TC:
        return MANET_PRIO_SIGNALLING;
    case MANET_FRAME_TEXT:
    case MANET_FRAME_POSITION:
    case MANET_FRAME_CONFIG:
        return MANET_PRIO_DATA;
    default:
        /* Unrecognised type. Relayed, never originated by this build, and given the
         * lowest class so an unknown frame can never pre-empt voice. */
        return MANET_PRIO_DATA;
    }
}

bool manet_header_ttl_decrement(manet_header_t *h)
{
    if (h == NULL || h->ttl == 0u) {
        return false;
    }
    h->ttl--;
    return h->ttl > 0u;
}
