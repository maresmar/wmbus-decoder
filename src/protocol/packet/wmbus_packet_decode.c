#include "wmbus_packet_decode.h"

#include <string.h>

#include "../decode/wmbus_decode.h"
#include "../frame/wmbus_frame.h"
#include "../parser/wmbus_parser.h"

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

bool wmbus_packet_decode_phy_frame(
    const WmBusPhyFrame* phy_frame,
    uint8_t* frame_buf,
    size_t frame_buf_max,
    WmBusPacketDecodeState* out) {
    if(!phy_frame || !frame_buf || frame_buf_max == 0U || !out) {
        return false;
    }
    if((phy_frame->mode == WmBusRxModeT && phy_frame->format != WmBusFrameFormatA) ||
       (phy_frame->mode == WmBusRxModeC && phy_frame->format != WmBusFrameFormatA &&
        phy_frame->format != WmBusFrameFormatB) ||
       (phy_frame->mode != WmBusRxModeT && phy_frame->mode != WmBusRxModeC)) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    const uint8_t* frame = NULL;
    size_t frame_len = 0U;

    if(wmbus_decode_is_plausible_frame(phy_frame->data, phy_frame->len)) {
        frame = phy_frame->data;
        frame_len = phy_frame->len;
        wmbus_packet_upgrade_quality(&out->quality, WmBusPacketQualityHeaderOk);
    }

    if(frame) {
        WmBusFrameMeasureResult measure = {0};
        if(wmbus_frame_measure(phy_frame->format, frame, frame_len, &measure)) {
            wmbus_packet_upgrade_quality_from_measure(&out->quality, &measure);
        }

        WmBusFrameNormalizeResult normalized_result = {0};
        if(wmbus_frame_normalize(
               phy_frame->format,
               frame,
               frame_len,
               frame_buf,
               frame_buf_max,
               &normalized_result)) {
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
