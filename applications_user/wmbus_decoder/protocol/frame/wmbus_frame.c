#include "wmbus_frame.h"

#include <stdio.h>
#include <string.h>

void wmbus_frame_decode_mfg(uint16_t man, char out[WMBUS_MFG_STR_LEN]) {
    out[0] = (char)(((man >> 10) & 0x1F) + ('A' - 1));
    out[1] = (char)(((man >> 5) & 0x1F) + ('A' - 1));
    out[2] = (char)((man & 0x1F) + ('A' - 1));
    out[3] = '\0';
    for(size_t i = 0; i < 3; i++) {
        if(out[i] < 'A' || out[i] > 'Z') {
            out[i] = '?';
        }
    }
}

bool wmbus_frame_format_id_bcd(const uint8_t id[4], char out[WMBUS_ID_STR_LEN]) {
    size_t pos = 0;
    for(int i = 3; i >= 0; i--) {
        uint8_t byte = id[i];
        uint8_t hi = (byte >> 4) & 0x0F;
        uint8_t lo = byte & 0x0F;
        if(hi > 9 || lo > 9) return false;
        out[pos++] = (char)('0' + hi);
        out[pos++] = (char)('0' + lo);
    }
    out[pos] = '\0';
    return true;
}

void wmbus_frame_format_id(const uint8_t id[4], char out[WMBUS_ID_STR_LEN], bool* is_bcd) {
    bool bcd = wmbus_frame_format_id_bcd(id, out);
    if(!bcd) {
        snprintf(
            out,
            WMBUS_ID_STR_LEN,
            "%02X%02X%02X%02X",
            id[3],
            id[2],
            id[1],
            id[0]);
    }
    if(is_bcd) {
        *is_bcd = bcd;
    }
}

static uint16_t wmbus_crc16_en13757(const uint8_t* data, size_t len) {
    uint16_t crc = 0x0000;
    for(size_t i = 0; i < len; i++) {
        uint8_t b = data[i];
        for(uint8_t bit = 0; bit < 8; bit++) {
            if((((crc & 0x8000) >> 8) ^ (b & 0x80)) != 0) {
                crc = (uint16_t)((crc << 1) ^ 0x3D65);
            } else {
                crc = (uint16_t)(crc << 1);
            }
            b <<= 1;
        }
    }
    return (uint16_t)(~crc);
}

static bool wmbus_frame_crc_check_a(const uint8_t* data, size_t len) {
    if(len < 12) return false;

    uint16_t calc = wmbus_crc16_en13757(data, 10);
    uint16_t check = ((uint16_t)data[10] << 8) | data[11];
    if(calc != check) return false;

    size_t pos = 12;
    while(pos + 18 <= len) {
        size_t to = pos + 16;
        calc = wmbus_crc16_en13757(&data[pos], 16);
        check = ((uint16_t)data[to] << 8) | data[to + 1];
        if(calc != check) return false;
        pos += 18;
    }

    if(pos < len - 2) {
        size_t tto = len - 2;
        size_t blen = tto - pos;
        calc = wmbus_crc16_en13757(&data[pos], blen);
        check = ((uint16_t)data[tto] << 8) | data[tto + 1];
        if(calc != check) return false;
    }

    return true;
}

static bool wmbus_frame_crc_check_b(const uint8_t* data, size_t len) {
    if(len < 12) return false;

    size_t crc1_pos = 0;
    size_t crc2_pos = 0;
    if(len <= 128) {
        crc1_pos = len - 2;
    } else {
        crc1_pos = 126;
        crc2_pos = len - 2;
    }

    uint16_t calc = wmbus_crc16_en13757(data, crc1_pos);
    uint16_t check = ((uint16_t)data[crc1_pos] << 8) | data[crc1_pos + 1];
    if(calc != check) return false;

    if(crc2_pos > 0) {
        size_t from2 = crc1_pos + 2;
        size_t len2 = crc2_pos - from2;
        calc = wmbus_crc16_en13757(&data[from2], len2);
        check = ((uint16_t)data[crc2_pos] << 8) | data[crc2_pos + 1];
        if(calc != check) return false;
    }

    return true;
}

size_t wmbus_frame_expected_len(uint8_t l_field, WmBusFrameFormat format) {
    if(format == WmBusFrameFormatA) {
        return wmbus_capture_frame_len_format_a(l_field);
    } else {
        return wmbus_capture_frame_len_format_b(l_field);
    }
}

bool wmbus_frame_build_format_a(
    const uint8_t* normalized,
    size_t normalized_len,
    uint8_t* out,
    size_t out_max,
    size_t* out_len) {
    if(!normalized || !out || !out_len) return false;
    if(normalized_len < 11) return false;
    if(!wmbus_capture_l_field_valid(normalized[0])) return false;

    size_t expected_normalized_len = (size_t)normalized[0] + 1U;
    if(normalized_len < expected_normalized_len) return false;

    size_t read = 0;
    size_t write = 0;

    if(out_max < wmbus_capture_frame_len_format_a(normalized[0])) return false;

    memcpy(out, normalized, 10);
    write = 10;
    read = 10;

    uint16_t crc = wmbus_crc16_en13757(normalized, 10);
    out[write++] = (uint8_t)(crc >> 8);
    out[write++] = (uint8_t)crc;

    while(read < expected_normalized_len) {
        size_t block_len = expected_normalized_len - read;
        if(block_len > 16U) block_len = 16U;

        memcpy(&out[write], &normalized[read], block_len);
        write += block_len;

        crc = wmbus_crc16_en13757(&normalized[read], block_len);
        out[write++] = (uint8_t)(crc >> 8);
        out[write++] = (uint8_t)crc;
        read += block_len;
    }

    *out_len = write;
    return true;
}

bool wmbus_frame_build_format_b(
    const uint8_t* normalized,
    size_t normalized_len,
    uint8_t* out,
    size_t out_max,
    size_t* out_len) {
    if(!normalized || !out || !out_len) return false;
    if(normalized_len < 11U || normalized_len > 126U) return false;
    if(!wmbus_capture_l_field_valid(normalized[0])) return false;
    if(normalized_len != (size_t)normalized[0] + 1U) return false;
    if(normalized_len + 2U > out_max) return false;

    memcpy(out, normalized, normalized_len);
    out[0] = (uint8_t)(normalized[0] + 2U);

    uint16_t crc = wmbus_crc16_en13757(out, normalized_len);
    out[normalized_len] = (uint8_t)(crc >> 8);
    out[normalized_len + 1U] = (uint8_t)crc;

    *out_len = normalized_len + 2U;
    return true;
}

static bool wmbus_frame_trim_crc_a(
    const uint8_t* data,
    size_t len,
    uint8_t* out,
    size_t out_max,
    size_t* out_len) {
    if(!data || !out || !out_len || len < 12) return false;
    if(out_max < 10) return false;

    size_t write = 0;
    memcpy(out, data, 10);
    write = 10;

    size_t pos = 12;
    while(pos + 18 <= len) {
        if(write + 16 > out_max) return false;
        memcpy(&out[write], &data[pos], 16);
        write += 16;
        pos += 18;
    }

    if(pos < len - 2) {
        size_t tto = len - 2;
        size_t blen = tto - pos;
        if(write + blen > out_max) return false;
        memcpy(&out[write], &data[pos], blen);
        write += blen;
    }

    if(write < 1 || write > 0x100U) return false;
    out[0] = (uint8_t)(write - 1U);
    *out_len = write;
    return true;
}

static bool wmbus_frame_trim_crc_b(
    const uint8_t* data,
    size_t len,
    uint8_t* out,
    size_t out_max,
    size_t* out_len) {
    if(!data || !out || !out_len || len < 12) return false;

    size_t crc1_pos = 0;
    size_t crc2_pos = 0;
    if(len <= 128) {
        crc1_pos = len - 2;
    } else {
        crc1_pos = 126;
        crc2_pos = len - 2;
    }

    size_t write = 0;
    if(crc1_pos > out_max) return false;
    memcpy(out, data, crc1_pos);
    write = crc1_pos;

    if(crc2_pos > 0) {
        size_t from2 = crc1_pos + 2;
        size_t len2 = crc2_pos - from2;
        if(write + len2 > out_max) return false;
        memcpy(&out[write], &data[from2], len2);
        write += len2;
    }

    if(write < 1 || write > 0x100U) return false;
    out[0] = (uint8_t)(write - 1U);
    *out_len = write;
    return true;
}

bool wmbus_frame_trim_crc(
    WmBusFrameFormat format,
    const uint8_t* data,
    size_t len,
    uint8_t* out,
    size_t out_max,
    size_t* out_len) {
    if(format == WmBusFrameFormatA) {
        return wmbus_frame_trim_crc_a(data, len, out, out_max, out_len);
    } else {
        return wmbus_frame_trim_crc_b(data, len, out, out_max, out_len);
    }
}

bool wmbus_frame_crc_check(WmBusFrameFormat format, const uint8_t* data, size_t len) {
    if(format == WmBusFrameFormatA) {
        return wmbus_frame_crc_check_a(data, len);
    } else {
        return wmbus_frame_crc_check_b(data, len);
    }
}

bool wmbus_frame_normalize(
    WmBusRxMode mode,
    const uint8_t* frame,
    size_t frame_len,
    uint8_t* normalized,
    size_t normalized_max,
    WmBusFrameNormalizeResult* out) {
    if(!frame || !normalized || !out || frame_len < 1) return false;

    memset(out, 0, sizeof(*out));

    WmBusFrameFormat ordered_formats[2];
    if(mode == WmBusRxModeT) {
        ordered_formats[0] = WmBusFrameFormatA;
        ordered_formats[1] = WmBusFrameFormatB;
    } else {
        // C capture strips any optional pre-frame signal byte before this point,
        // so normalization expects Link-B frame bytes beginning at the L-field.
        // Prefer format B first, but only a passing CRC is allowed to normalize.
        ordered_formats[0] = WmBusFrameFormatB;
        ordered_formats[1] = WmBusFrameFormatA;
    }

    uint8_t candidate_trimmed[256] = {0};
    size_t candidate_max = normalized_max;
    if(candidate_max > sizeof(candidate_trimmed)) {
        candidate_max = sizeof(candidate_trimmed);
    }
    uint8_t l_field = frame[0];

    for(size_t i = 0; i < 2; i++) {
        WmBusFrameFormat format = ordered_formats[i];
        size_t expected_len = wmbus_frame_expected_len(l_field, format);
        if(frame_len < expected_len) continue;

        size_t trimmed_len = 0;
        if(!wmbus_frame_trim_crc(
               format, frame, expected_len, candidate_trimmed, candidate_max, &trimmed_len)) {
            continue;
        }

        out->crc_known = true;
        if(expected_len >= out->computed_len) {
            out->format = format;
            out->computed_len = expected_len;
            out->normalized_len = trimmed_len;
        }

        if(wmbus_frame_crc_check(format, frame, expected_len)) {
            memcpy(normalized, candidate_trimmed, trimmed_len);
            out->length_ok = true;
            out->crc_ok = true;
            out->format = format;
            out->computed_len = expected_len;
            out->normalized_len = trimmed_len;
            return true;
        }
    }

    return false;
}
