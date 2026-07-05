#include "wmbus_packet_decode.h"

#include <string.h>

#include "../decode/wmbus_decode.h"
#include "../frame/wmbus_frame.h"
#include "../parser/wmbus_parser.h"

#define WMBUS_DECODE_MAX         256U
#define WMBUS_T_SYNC_SEARCH_BITS 8U

typedef struct {
    WmBusPacketQuality quality;
    size_t frame_len;
    int best_offset;
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
        WmBusPacketEllData ell = {
            .has_ell = true,
            .header_len = 11U,
        };
        ell.ci_field = frame[pos++];
        ell.cc = frame[pos++];
        ell.acc = frame[pos++];

        if(ell.ci_field == 0x8EU || ell.ci_field == 0x8FU) {
            if(frame_len < pos + 8U) {
                return;
            }
            pos += 8U;
        }

        if(wmbus_parser_ell_has_session_fields(ell.ci_field)) {
            if(frame_len < pos + 6U) {
                return;
            }
            ell.has_session = true;
            ell.sn = (uint32_t)frame[pos] | ((uint32_t)frame[pos + 1U] << 8U) |
                     ((uint32_t)frame[pos + 2U] << 16U) | ((uint32_t)frame[pos + 3U] << 24U);
            ell.security_mode = wmbus_parser_ell_security_mode(ell.sn);
            pos += 4U;
            ell.payload_crc = (uint16_t)frame[pos] | ((uint16_t)frame[pos + 1U] << 8U);
            pos += 2U;
        }

        if(pos <= UINT8_MAX) {
            ell.header_len = (uint8_t)pos;
            record->ell = ell;
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
    if(candidate->quality != WmBusPacketQualityAnyCapture) score += 1;
    if(wmbus_packet_quality_meets(candidate->quality, WmBusPacketQualityHeaderOk)) score += 4;
    if(wmbus_packet_quality_meets(candidate->quality, WmBusPacketQualityFrameComplete)) score += 2;
    if(wmbus_packet_quality_meets(candidate->quality, WmBusPacketQualityCrcOk)) score += 1;
    return score;
}

static void
    wmbus_packet_upgrade_quality(WmBusPacketQuality* quality, WmBusPacketQuality candidate) {
    if(!quality) return;
    if(wmbus_packet_quality_meets(candidate, *quality)) {
        *quality = candidate;
    }
}

static void wmbus_packet_upgrade_quality_from_normalize(
    WmBusPacketQuality* quality,
    const WmBusFrameNormalizeResult* normalize) {
    if(!quality || !normalize) return;
    if(normalize->length_ok) {
        wmbus_packet_upgrade_quality(quality, WmBusPacketQualityFrameComplete);
    }
    if(normalize->crc_known && normalize->crc_ok) {
        wmbus_packet_upgrade_quality(quality, WmBusPacketQualityCrcOk);
    }
}

static void wmbus_packet_upgrade_quality_from_measure(
    WmBusPacketQuality* quality,
    const WmBusFrameMeasureResult* measure) {
    if(!quality || !measure) return;
    if(measure->complete) {
        wmbus_packet_upgrade_quality(quality, WmBusPacketQualityFrameComplete);
    }
}

static bool wmbus_packet_decode_copy_frame(
    const uint8_t* frame,
    size_t frame_len,
    uint8_t* frame_buf,
    size_t frame_buf_max,
    const uint8_t** out_frame,
    size_t* out_frame_len) {
    if(!frame || frame_len == 0U || !frame_buf || frame_buf_max == 0U || !out_frame ||
       !out_frame_len) {
        return false;
    }

    size_t copy_len = frame_len > frame_buf_max ? frame_buf_max : frame_len;
    if(frame != frame_buf) {
        memcpy(frame_buf, frame, copy_len);
    }
    *out_frame = frame_buf;
    *out_frame_len = copy_len;
    return true;
}

static bool wmbus_try_decode_t_candidate(
    const WmBusCaptureFrame* capture,
    size_t bit_offset,
    WmBusTDecodeResult* result) {
    if(!capture || !result) return false;

    size_t raw_bit_len = capture->len * 8U;

    memset(result, 0, sizeof(*result));
    result->best_offset = -1;

    uint8_t decoded[WMBUS_DECODE_MAX] = {0};
    size_t decoded_len = 0;
    size_t l_bit_len = bit_offset + 12U;
    if(raw_bit_len < l_bit_len ||
       !wmbus_decode_3of6_bits(capture->data, l_bit_len, bit_offset, decoded, 1U, &decoded_len) ||
       decoded_len != 1U) {
        return false;
    }

    uint8_t l_field = decoded[0];
    if(!wmbus_frame_l_field_valid(l_field)) return false;

    size_t expected_len = wmbus_frame_len_format_a(l_field);
    size_t expected_bit_len = bit_offset + expected_len * 12U;
    if(raw_bit_len < expected_bit_len) return false;

    if(!wmbus_decode_3of6_bits(
           capture->data, expected_bit_len, bit_offset, decoded, sizeof(decoded), &decoded_len) ||
       decoded_len < expected_len) {
        return false;
    }

    if(!wmbus_decode_is_plausible_frame(decoded, decoded_len)) return false;
    result->best_offset = (int)bit_offset;
    wmbus_packet_upgrade_quality(&result->quality, WmBusPacketQualityHeaderOk);

    const uint8_t* frame = decoded;
    size_t frame_len = expected_len;
    WmBusFrameMeasureResult measure = {0};
    if(wmbus_frame_measure(WmBusRxModeT, decoded, expected_len, &measure)) {
        frame_len = measure.frame_len;
        wmbus_packet_upgrade_quality_from_measure(&result->quality, &measure);
    }

    uint8_t normalized[WMBUS_DECODE_MAX] = {0};
    WmBusFrameNormalizeResult normalized_result = {0};
    if(wmbus_frame_normalize(
           WmBusRxModeT,
           decoded,
           expected_len,
           normalized,
           sizeof(normalized),
           &normalized_result)) {
        frame = normalized;
        frame_len = normalized_result.normalized_len;
        wmbus_packet_upgrade_quality_from_normalize(&result->quality, &normalized_result);
    }

    result->frame_len = frame_len;
    memcpy(result->frame, frame, frame_len);
    return true;
}

static void wmbus_decode_t_capture(const WmBusCaptureFrame* capture, WmBusTDecodeResult* result) {
    if(!capture || !result) return;

    memset(result, 0, sizeof(*result));
    result->best_offset = -1;

    int best_score = -1;

    size_t raw_bit_len = capture->len * 8U;
    // Sync-based RX should put FIFO at frame data. Scan only the possible bit
    // alignments within the first byte; wider prefix recovery belongs in radio
    // validation, not packet normalization.
    size_t scan_bits = raw_bit_len;
    if(scan_bits > WMBUS_T_SYNC_SEARCH_BITS) {
        scan_bits = WMBUS_T_SYNC_SEARCH_BITS;
    }

    for(size_t bit_offset = 0; bit_offset < scan_bits; bit_offset++) {
        WmBusTDecodeResult candidate = {0};
        if(!wmbus_try_decode_t_candidate(capture, bit_offset, &candidate)) {
            continue;
        }

        int score = wmbus_score_t_decode_candidate(&candidate);
        bool better = false;
        if(score > best_score) {
            better = true;
        } else if(score == best_score && score >= 0) {
            if(result->best_offset < 0 || candidate.best_offset < result->best_offset) {
                better = true;
            }
        }

        if(better) {
            *result = candidate;
            best_score = score;
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
    bool use_3of6 = (capture->mode == WmBusRxModeT);

    const uint8_t* frame = NULL;
    size_t frame_len = 0U;

    if(use_3of6) {
        WmBusTDecodeResult t_result = {0};
        wmbus_decode_t_capture(capture, &t_result);
        record->best_offset = t_result.best_offset;
        out->quality = t_result.quality;

        if(wmbus_packet_quality_meets(t_result.quality, WmBusPacketQualityHeaderOk)) {
            wmbus_packet_decode_copy_frame(
                t_result.frame, t_result.frame_len, frame_buf, frame_buf_max, &frame, &frame_len);
        }
    } else {
        if(wmbus_decode_is_plausible_frame(capture->data, capture->len)) {
            frame = capture->data;
            frame_len = capture->len;
            wmbus_packet_upgrade_quality(&out->quality, WmBusPacketQualityHeaderOk);
        }
    }

    if(frame && !use_3of6) {
        WmBusFrameMeasureResult measure = {0};
        if(wmbus_frame_measure(capture->mode, frame, frame_len, &measure)) {
            wmbus_packet_upgrade_quality_from_measure(&out->quality, &measure);
        }

        WmBusFrameNormalizeResult normalized_result = {0};
        if(wmbus_frame_normalize(
               capture->mode, frame, frame_len, frame_buf, frame_buf_max, &normalized_result)) {
            frame = frame_buf;
            frame_len = normalized_result.normalized_len;
            wmbus_packet_upgrade_quality_from_normalize(&out->quality, &normalized_result);
        } else if(measure.complete && measure.frame_len <= frame_buf_max) {
            wmbus_packet_decode_copy_frame(
                frame, measure.frame_len, frame_buf, frame_buf_max, &frame, &frame_len);
        }
    }

    out->frame = frame;
    out->frame_len = frame_len;
    return true;
}

void wmbus_packet_store_frame(WmBusPacketRecord* record, const uint8_t* frame, size_t frame_len) {
    if(!record || !frame || frame_len == 0U) return;

    record->packet_len = (uint16_t)((frame_len > sizeof(record->packet_bytes)) ?
                                        sizeof(record->packet_bytes) :
                                        frame_len);
    memcpy(record->packet_bytes, frame, record->packet_len);
    wmbus_packet_extract_dll_tpl_info(frame, frame_len, record);
}
