#include "wmbus_packet_decode.h"

#include <string.h>

#include "../decode/wmbus_decode.h"
#include "../frame/wmbus_frame.h"
#include "../parser/wmbus_parser.h"

#define WMBUS_DECODE_MAX 256U

typedef struct {
    bool decoded_ok;
    bool plausible;
    bool length_ok;
    bool crc_known;
    bool crc_ok;
    bool normalize_format_known;
    size_t frame_len;
    int best_offset;
    WmBusFrameFormat normalize_format;
    uint8_t frame[WMBUS_DECODE_MAX];
} WmBusTDecodeResult;

static void wmbus_packet_populate_identity(WmBusPacketRecord* record) {
    if(!record) return;

    wmbus_frame_decode_mfg(record->dll.m_field, record->identity.manufacturer);
    wmbus_frame_format_id(
        record->dll.id, record->identity.meter_id, &record->identity.meter_id_is_bcd);
}

static bool wmbus_packet_ci_has_short_tpl(uint8_t ci) {
    switch(ci) {
    case 0x5A:
    case 0x61:
    case 0x65:
    case 0x67:
    case 0x6E:
    case 0x74:
    case 0x7A:
    case 0x7D:
    case 0x7F:
    case 0x8A:
    case 0x9E:
        return true;
    default:
        return false;
    }
}

static uint8_t wmbus_packet_header_payload_offset(const WmBusPacketRecord* record) {
    uint8_t offset = 11U;
    if(!record) {
        return offset;
    }
    if(record->ell.has_ell && record->ell.header_len > offset) {
        offset = record->ell.header_len;
    }
    if(record->tpl.has_short_tpl && record->tpl.header_len > offset) {
        offset = record->tpl.header_len;
    }
    return offset;
}

static void wmbus_packet_extract_dll_tpl_info(
    const uint8_t* frame,
    size_t frame_len,
    WmBusPacketRecord* record) {
    if(!frame || !record || frame_len < 11U) return;

    record->dll.l_field = frame[0];
    record->dll.c_field = frame[1];
    record->dll.m_field = (uint16_t)frame[2] | ((uint16_t)frame[3] << 8);
    memcpy(record->dll.id, &frame[4], sizeof(record->dll.id));
    record->dll.version = frame[8];
    record->dll.dev_type = frame[9];
    record->dll.ci_field = frame[10];
    wmbus_packet_populate_identity(record);
    record->tpl.header_len = 11U;
    record->tpl.security_mode = 0U;
    record->ell.header_len = 11U;

    if(frame_len >= 13U && wmbus_parser_ci_has_ell(frame[10])) {
        size_t pos = 10U;
        record->ell.has_ell = true;
        record->ell.ci_field = frame[pos++];
        record->ell.cc = frame[pos++];
        record->ell.acc = frame[pos++];

        if(record->ell.ci_field == 0x8EU || record->ell.ci_field == 0x8FU) {
            if(frame_len < pos + 8U) {
                return;
            }
            pos += 8U;
        }

        if(wmbus_parser_ell_has_session_fields(record->ell.ci_field)) {
            if(frame_len < pos + 6U) {
                return;
            }
            record->ell.has_session = true;
            record->ell.sn = (uint32_t)frame[pos] | ((uint32_t)frame[pos + 1U] << 8U) |
                             ((uint32_t)frame[pos + 2U] << 16U) |
                             ((uint32_t)frame[pos + 3U] << 24U);
            record->ell.security_mode = wmbus_parser_ell_security_mode(record->ell.sn);
            pos += 4U;
            record->ell.payload_crc =
                (uint16_t)frame[pos] | ((uint16_t)frame[pos + 1U] << 8U);
            pos += 2U;
        }

        if(pos <= UINT8_MAX) {
            record->ell.header_len = (uint8_t)pos;
        }
    }

    if(frame_len >= 15U && wmbus_packet_ci_has_short_tpl(frame[10])) {
        record->tpl.has_short_tpl = true;
        record->tpl.header_len = 15U;
        record->tpl.acc = frame[11];
        record->tpl.tpl_status = frame[12];
        record->tpl.cfg = (uint16_t)frame[13] | ((uint16_t)frame[14] << 8);
        record->tpl.security_mode = wmbus_parser_short_tpl_security_mode(record->tpl.cfg);
    }

    record->tpl.header_len = wmbus_packet_header_payload_offset(record);
}

static int wmbus_score_t_decode_candidate(const WmBusTDecodeResult* candidate) {
    int score = 0;
    if(candidate->decoded_ok) score += 1;
    if(candidate->plausible) score += 4;
    if(candidate->length_ok) score += 2;
    if(candidate->crc_ok) score += 1;
    return score;
}

static bool wmbus_try_decode_t_candidate(
    const WmBusCaptureFrame* capture,
    uint8_t bit_offset,
    uint8_t tail_pad,
    WmBusTDecodeResult* result) {
    if(!capture || !result) return false;

    size_t raw_bit_len = capture->len * 8U;
    if(raw_bit_len <= tail_pad) return false;
    raw_bit_len -= tail_pad;

    memset(result, 0, sizeof(*result));
    result->best_offset = bit_offset;

    uint8_t decoded[WMBUS_DECODE_MAX] = {0};
    size_t decoded_len = 0;
    if(!wmbus_decode_3of6_bits(
           capture->data, raw_bit_len, bit_offset, decoded, sizeof(decoded), &decoded_len)) {
        return false;
    }

    result->decoded_ok = true;
    result->plausible = wmbus_decode_is_plausible_frame(decoded, decoded_len);
    if(!result->plausible) return true;

    const uint8_t* frame = decoded;
    size_t frame_len = decoded_len;
    uint8_t normalized[WMBUS_DECODE_MAX] = {0};
    WmBusFrameNormalizeResult normalized_result = {0};
    if(wmbus_frame_normalize(
           WmBusRxModeT, decoded, decoded_len, normalized, sizeof(normalized), &normalized_result)) {
        frame = normalized;
        frame_len = normalized_result.normalized_len;
        result->normalize_format_known = true;
        result->normalize_format = normalized_result.format;
    }

    result->length_ok = normalized_result.length_ok;
    result->crc_known = normalized_result.crc_known;
    result->crc_ok = normalized_result.crc_ok;
    result->frame_len = frame_len;
    memcpy(result->frame, frame, frame_len);
    return true;
}

static void wmbus_decode_t_capture(const WmBusCaptureFrame* capture, WmBusTDecodeResult* result) {
    if(!capture || !result) return;

    memset(result, 0, sizeof(*result));
    result->best_offset = -1;

    int best_score = -1;
    uint8_t best_tail_pad = 0xFFU;

    for(uint8_t bit_offset = 0; bit_offset < 8U; bit_offset++) {
        for(uint8_t tail_pad = 0; tail_pad < 8U; tail_pad++) {
            WmBusTDecodeResult candidate = {0};
            if(!wmbus_try_decode_t_candidate(capture, bit_offset, tail_pad, &candidate)) {
                continue;
            }

            int score = wmbus_score_t_decode_candidate(&candidate);
            bool better = false;
            if(score > best_score) {
                better = true;
            } else if(score == best_score && score >= 0) {
                if(result->best_offset < 0 || candidate.best_offset < result->best_offset) {
                    better = true;
                } else if(candidate.best_offset == result->best_offset && tail_pad < best_tail_pad) {
                    better = true;
                }
            }

            if(better) {
                *result = candidate;
                best_score = score;
                best_tail_pad = tail_pad;
            }
        }
    }
}

bool wmbus_packet_decode_capture(
    const WmBusCaptureFrame* capture,
    WmBusPacketRecord* record,
    uint8_t* frame_buf,
    size_t frame_buf_max,
    WmBusPacketDecodeState* out) {
    if(!capture || !record || !frame_buf || frame_buf_max == 0U || !out) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    out->used_3of6 = (capture->mode == WmBusRxModeT);

    const uint8_t* frame = NULL;
    size_t frame_len = 0U;

    if(out->used_3of6) {
        WmBusTDecodeResult t_result = {0};
        wmbus_decode_t_capture(capture, &t_result);
        record->decoded_ok = t_result.decoded_ok;
        record->plausible = t_result.plausible;
        record->length_ok = t_result.length_ok;
        record->crc_known = t_result.crc_known;
        record->crc_ok = t_result.crc_ok;
        record->normalize_format_known = t_result.normalize_format_known;
        record->normalize_format = t_result.normalize_format;
        record->best_offset = t_result.best_offset;

        if(t_result.plausible) {
            frame = t_result.frame;
            frame_len = t_result.frame_len;
        }
    } else {
        record->decoded_ok = true;
        if(wmbus_decode_is_plausible_frame(capture->data, capture->len)) {
            record->plausible = true;
            frame = capture->data;
            frame_len = capture->len;
        }
    }

    if(record->plausible && !out->used_3of6) {
        WmBusFrameNormalizeResult normalized_result = {0};
        if(wmbus_frame_normalize(
               capture->mode,
               frame,
               frame_len,
               frame_buf,
               frame_buf_max,
               &normalized_result)) {
            frame = frame_buf;
            frame_len = normalized_result.normalized_len;
            record->normalize_format_known = true;
            record->normalize_format = normalized_result.format;
        }
        record->length_ok = normalized_result.length_ok;
        record->crc_known = normalized_result.crc_known;
        record->crc_ok = normalized_result.crc_ok;
    } else if(record->plausible && frame_len > 0U) {
        size_t copy_len = frame_len > frame_buf_max ? frame_buf_max : frame_len;
        memcpy(frame_buf, frame, copy_len);
        frame = frame_buf;
        frame_len = copy_len;
    }

    out->frame = frame;
    out->frame_len = frame_len;
    return true;
}

void wmbus_packet_store_frame(
    WmBusPacketRecord* record,
    const uint8_t* frame,
    size_t frame_len) {
    if(!record || !frame || frame_len == 0U) return;

    record->packet_is_frame = true;
    record->packet_len =
        (uint16_t)((frame_len > sizeof(record->packet_bytes)) ? sizeof(record->packet_bytes) :
                                                              frame_len);
    memcpy(record->packet_bytes, frame, record->packet_len);
    wmbus_packet_extract_dll_tpl_info(frame, frame_len, record);
}
