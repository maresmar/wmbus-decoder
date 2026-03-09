#include "wmbus_selftest.h"

#include "wmbus_config.h"
#include "wmbus_parser.h"

#include <furi.h>

#include <stdio.h>
#include <string.h>

#define TAG "WmBusSelftest"

static const uint8_t wmbus_apator_a[] = {
    0x6E, 0x44, 0x01, 0x06, 0x20, 0x20, 0x20, 0x20, 0x05, 0x07, 0x7A, 0x9A,
    0x00, 0x60, 0x85, 0x2F, 0x2F, 0x0F, 0x0A, 0x73, 0x43, 0x93, 0xCC, 0x00,
    0x00, 0x43, 0x5B, 0x01, 0x83, 0x00, 0x1A, 0x54, 0xE0, 0x6F, 0x63, 0x02,
    0x91, 0x34, 0x25, 0x10, 0x03, 0x0F, 0x00, 0x00, 0x7B, 0x01, 0x3E, 0x0B,
    0x00, 0x00, 0x3E, 0x0B, 0x00, 0x00, 0x3E, 0x0B, 0x00, 0x00, 0x3E, 0x0B,
    0x00, 0x00, 0x3E, 0x0B, 0x00, 0x00, 0x3E, 0x0B, 0x00, 0x00, 0x3E, 0x0B,
    0x00, 0x00, 0x65, 0x00, 0x00, 0x00, 0x3D, 0x00, 0x00, 0x00, 0x3D, 0x00,
    0x00, 0x00, 0x3D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA0, 0x91,
    0x0C, 0xB0, 0x03, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xA6, 0x2B,
};

static const uint8_t wmbus_apator_b[] = {
    0x4E, 0x44, 0x01, 0x06, 0x20, 0x20, 0x20, 0x21, 0x05, 0x07, 0x7A, 0x13,
    0x00, 0x40, 0x85, 0x2F, 0x2F, 0x0F, 0x6D, 0x4C, 0x38, 0x93, 0x00, 0x02,
    0x00, 0x43, 0x84, 0x02, 0x10, 0x35, 0x1F, 0x04, 0x00, 0x75, 0x01, 0x2C,
    0x0B, 0x04, 0x00, 0x48, 0xD6, 0x03, 0x00, 0x3E, 0x63, 0x03, 0x00, 0xCD,
    0x2C, 0x03, 0x00, 0x1E, 0xF4, 0x02, 0x00, 0x0A, 0xCE, 0x02, 0x00, 0xA0,
    0x98, 0xA3, 0x96, 0x03, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x19, 0x77,
};

static const uint8_t wmbus_apator_c[] = {
    0x3E, 0x44, 0x01, 0x06, 0x14, 0x05, 0x41, 0x03, 0x05, 0x07, 0x7A, 0x19,
    0x00, 0x30, 0x85, 0x2F, 0x2F, 0x0F, 0x86, 0xB4, 0xB8, 0x95, 0x29, 0x02,
    0x00, 0x40, 0xC6, 0xC1, 0xB4, 0xF0, 0xF3, 0xF3, 0x41, 0x55, 0x59, 0x42,
    0xFA, 0x70, 0x10, 0x00, 0xF0, 0x01, 0x01, 0x00, 0x00, 0x10, 0xBC, 0x78,
    0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x24, 0x83,
};

static const WmBusTestVector wmbus_vector_c_apator_a_ok = {
    .name = "c_apator_a_ok",
    .data = wmbus_apator_a,
    .len = sizeof(wmbus_apator_a),
    .is_t_raw = false,
    .expect_plausible = true,
    .expect_crc_ok = true,
    .expected_offset = 0,
};

static const WmBusTestVector wmbus_vector_c_apator_b_ok = {
    .name = "c_apator_b_ok",
    .data = wmbus_apator_b,
    .len = sizeof(wmbus_apator_b),
    .is_t_raw = false,
    .expect_plausible = true,
    .expect_crc_ok = true,
    .expected_offset = 0,
};

static const WmBusTestVector wmbus_vector_c_apator_c_ok = {
    .name = "c_apator_c_ok",
    .data = wmbus_apator_c,
    .len = sizeof(wmbus_apator_c),
    .is_t_raw = false,
    .expect_plausible = true,
    .expect_crc_ok = true,
    .expected_offset = 0,
};

static const WmBusTestVector wmbus_vector_c_apator_b_crc_bad = {
    .name = "c_apator_b_crc_bad",
    .data = wmbus_apator_b,
    .len = sizeof(wmbus_apator_b),
    .is_t_raw = false,
    .expect_plausible = true,
    .expect_crc_ok = false,
    .expected_offset = 0,
};

static const WmBusTestVector wmbus_vector_c_apator_b_bad_c_field = {
    .name = "c_apator_b_bad_c_field",
    .data = wmbus_apator_b,
    .len = sizeof(wmbus_apator_b),
    .is_t_raw = false,
    .expect_plausible = false,
    .expect_crc_ok = false,
    .expected_offset = 0,
};

static const WmBusTestVector wmbus_vector_t_apator_a_off1_ok = {
    .name = "t_apator_a_off1_ok",
    .data = wmbus_apator_a,
    .len = sizeof(wmbus_apator_a),
    .is_t_raw = true,
    .expect_plausible = true,
    .expect_crc_ok = true,
    .expected_offset = 1,
};

static const WmBusTestVector wmbus_vector_t_apator_b_off3_ok = {
    .name = "t_apator_b_off3_ok",
    .data = wmbus_apator_b,
    .len = sizeof(wmbus_apator_b),
    .is_t_raw = true,
    .expect_plausible = true,
    .expect_crc_ok = true,
    .expected_offset = 3,
};

static const WmBusTestVector wmbus_vector_t_apator_c_off7_ok = {
    .name = "t_apator_c_off7_ok",
    .data = wmbus_apator_c,
    .len = sizeof(wmbus_apator_c),
    .is_t_raw = true,
    .expect_plausible = true,
    .expect_crc_ok = true,
    .expected_offset = 7,
};

static const WmBusTestVector wmbus_vector_t_apator_b_off5_crc_bad = {
    .name = "t_apator_b_off5_crc_bad",
    .data = wmbus_apator_b,
    .len = sizeof(wmbus_apator_b),
    .is_t_raw = true,
    .expect_plausible = true,
    .expect_crc_ok = false,
    .expected_offset = 5,
};

static const WmBusTestVector wmbus_vector_t_apator_b_off2_bad_symbol = {
    .name = "t_apator_b_off2_bad_symbol",
    .data = wmbus_apator_b,
    .len = sizeof(wmbus_apator_b),
    .is_t_raw = true,
    .expect_plausible = false,
    .expect_crc_ok = false,
    .expected_offset = 2,
};

static const WmBusSelftestCase wmbus_selftest_cases[] = {
    {
        .name = "c_apator_a_ok",
        .vector = &wmbus_vector_c_apator_a_ok,
        .build_format_a = true,
        .seed_corrupt_byte_pos = WMBUS_BIT_NONE,
        .frame_corrupt_byte_pos = WMBUS_BIT_NONE,
        .raw_corrupt_bit_pos = WMBUS_BIT_NONE,
    },
    {
        .name = "c_apator_b_ok",
        .vector = &wmbus_vector_c_apator_b_ok,
        .build_format_a = true,
        .seed_corrupt_byte_pos = WMBUS_BIT_NONE,
        .frame_corrupt_byte_pos = WMBUS_BIT_NONE,
        .raw_corrupt_bit_pos = WMBUS_BIT_NONE,
    },
    {
        .name = "c_apator_c_ok",
        .vector = &wmbus_vector_c_apator_c_ok,
        .build_format_a = true,
        .seed_corrupt_byte_pos = WMBUS_BIT_NONE,
        .frame_corrupt_byte_pos = WMBUS_BIT_NONE,
        .raw_corrupt_bit_pos = WMBUS_BIT_NONE,
    },
    {
        .name = "c_apator_b_crc_bad",
        .vector = &wmbus_vector_c_apator_b_crc_bad,
        .build_format_a = true,
        .seed_corrupt_byte_pos = WMBUS_BIT_NONE,
        .frame_corrupt_byte_pos = WMBUS_BYTE_LAST,
        .raw_corrupt_bit_pos = WMBUS_BIT_NONE,
    },
    {
        .name = "c_apator_b_bad_c_field",
        .vector = &wmbus_vector_c_apator_b_bad_c_field,
        .build_format_a = true,
        .seed_corrupt_byte_pos = 1U,
        .frame_corrupt_byte_pos = WMBUS_BIT_NONE,
        .raw_corrupt_bit_pos = WMBUS_BIT_NONE,
    },
    {
        .name = "t_apator_a_off1_ok",
        .vector = &wmbus_vector_t_apator_a_off1_ok,
        .build_format_a = true,
        .seed_corrupt_byte_pos = WMBUS_BIT_NONE,
        .frame_corrupt_byte_pos = WMBUS_BIT_NONE,
        .raw_corrupt_bit_pos = WMBUS_BIT_NONE,
    },
    {
        .name = "t_apator_b_off3_ok",
        .vector = &wmbus_vector_t_apator_b_off3_ok,
        .build_format_a = true,
        .seed_corrupt_byte_pos = WMBUS_BIT_NONE,
        .frame_corrupt_byte_pos = WMBUS_BIT_NONE,
        .raw_corrupt_bit_pos = WMBUS_BIT_NONE,
    },
    {
        .name = "t_apator_c_off7_ok",
        .vector = &wmbus_vector_t_apator_c_off7_ok,
        .build_format_a = true,
        .seed_corrupt_byte_pos = WMBUS_BIT_NONE,
        .frame_corrupt_byte_pos = WMBUS_BIT_NONE,
        .raw_corrupt_bit_pos = WMBUS_BIT_NONE,
    },
    {
        .name = "t_apator_b_off5_crc_bad",
        .vector = &wmbus_vector_t_apator_b_off5_crc_bad,
        .build_format_a = true,
        .seed_corrupt_byte_pos = WMBUS_BIT_NONE,
        .frame_corrupt_byte_pos = WMBUS_BYTE_LAST,
        .raw_corrupt_bit_pos = WMBUS_BIT_NONE,
    },
    {
        .name = "t_apator_b_off2_bad_symbol",
        .vector = &wmbus_vector_t_apator_b_off2_bad_symbol,
        .build_format_a = true,
        .seed_corrupt_byte_pos = WMBUS_BIT_NONE,
        .frame_corrupt_byte_pos = WMBUS_BIT_NONE,
        .raw_corrupt_bit_pos = wmbus_vector_t_apator_b_off2_bad_symbol.expected_offset,
    },
};

static const uint8_t wmbus_3of6_encode_lut[16] = {
    0x16,
    0x0D,
    0x0E,
    0x0B,
    0x1C,
    0x19,
    0x1A,
    0x13,
    0x2C,
    0x25,
    0x26,
    0x23,
    0x34,
    0x31,
    0x32,
    0x29,
};

typedef struct {
    uint8_t seed[WMBUS_SELFTEST_BUF_MAX];
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX];
    uint8_t raw[WMBUS_SELFTEST_BUF_MAX];
    uint8_t shifted[WMBUS_SELFTEST_BUF_MAX];
    uint8_t decoded[WMBUS_SELFTEST_BUF_MAX];
    uint8_t normalized[WMBUS_SELFTEST_BUF_MAX];
} WmBusSelftestScratch;

static WmBusSelftestScratch wmbus_selftest_scratch;

static void wmbus_selftest_result_reset(WmBusSelftestResult* result) {
    memset(result, 0, sizeof(*result));
    result->best_offset = -1;
    memcpy(result->manufacturer, "???", WMBUS_MFG_STR_LEN);
    memcpy(result->id, "????????", WMBUS_ID_STR_LEN);
}

static void wmbus_selftest_set_identity(
    const uint8_t* frame,
    size_t frame_len,
    WmBusSelftestResult* result) {
    if(frame_len < 8) return;

    uint16_t man = (uint16_t)frame[2] | ((uint16_t)frame[3] << 8);
    wmbus_frame_decode_mfg(man, result->manufacturer);
    wmbus_frame_format_id(&frame[4], result->id, NULL);
    result->has_identity = true;
}

static bool wmbus_selftest_guess_computed_len(
    WmBusRxMode mode,
    const uint8_t* frame,
    size_t frame_len,
    size_t* computed_len) {
    if(!frame || !computed_len || frame_len < 1) return false;
    if(!wmbus_capture_l_field_valid(frame[0])) return false;

    WmBusFrameFormat ordered_formats[2];
    if(mode == WmBusRxModeT) {
        ordered_formats[0] = WmBusFrameFormatA;
        ordered_formats[1] = WmBusFrameFormatB;
    } else {
        ordered_formats[0] = WmBusFrameFormatB;
        ordered_formats[1] = WmBusFrameFormatA;
    }

    size_t preferred_len = wmbus_frame_expected_len(frame[0], ordered_formats[0]);
    for(size_t i = 0; i < 2; i++) {
        size_t expected_len = wmbus_frame_expected_len(frame[0], ordered_formats[i]);
        if(frame_len >= expected_len) {
            *computed_len = expected_len;
            return true;
        }
    }

    *computed_len = preferred_len;
    return true;
}

static void wmbus_selftest_analyze_decoded(
    WmBusRxMode mode,
    const uint8_t* frame,
    size_t frame_len,
    WmBusSelftestResult* result) {
    wmbus_selftest_result_reset(result);
    result->decoded_ok = true;

    if(frame_len < 1) return;

    result->l_field = frame[0];
    result->has_l_field = true;
    wmbus_selftest_guess_computed_len(mode, frame, frame_len, &result->computed_len);
    result->has_computed_len = wmbus_capture_l_field_valid(frame[0]);
    wmbus_selftest_set_identity(frame, frame_len, result);

    result->plausible = wmbus_parser_is_plausible(frame, frame_len);
    if(!result->plausible) return;

    WmBusFrameNormalizeResult normalized_result = {0};
    bool normalized_ok = wmbus_frame_normalize(
        mode,
        frame,
        frame_len,
        wmbus_selftest_scratch.normalized,
        sizeof(wmbus_selftest_scratch.normalized),
        &normalized_result);

    result->length_ok = normalized_result.length_ok;
    result->crc_ok = normalized_result.crc_ok;
    if(normalized_result.computed_len > 0) {
        result->computed_len = normalized_result.computed_len;
        result->has_computed_len = true;
    }

    if(normalized_ok) {
        wmbus_selftest_set_identity(
            wmbus_selftest_scratch.normalized, normalized_result.normalized_len, result);
    }
}

static void wmbus_selftest_corrupt_byte(uint8_t* data, size_t len, size_t byte_pos) {
    if(!data || byte_pos == WMBUS_BIT_NONE) return;
    if(byte_pos == WMBUS_BYTE_LAST) {
        if(len == 0) return;
        byte_pos = len - 1U;
    }
    if(byte_pos >= len) return;
    data[byte_pos] ^= 0x01U;
}

static void wmbus_selftest_apply_frame_corruption(
    const uint8_t* frame,
    size_t frame_len,
    size_t frame_corrupt_byte_pos) {
    if(frame == wmbus_selftest_scratch.frame) {
        wmbus_selftest_corrupt_byte(
            wmbus_selftest_scratch.frame, frame_len, frame_corrupt_byte_pos);
    } else if(frame == wmbus_selftest_scratch.seed) {
        wmbus_selftest_corrupt_byte(
            wmbus_selftest_scratch.seed, frame_len, frame_corrupt_byte_pos);
    }
}

static bool wmbus_selftest_prepare_frame(
    const WmBusSelftestCase* test_case,
    const uint8_t** frame,
    size_t* frame_len) {
    furi_check(test_case);
    furi_check(test_case->vector);
    furi_check(frame);
    furi_check(frame_len);

    if(test_case->vector->len > sizeof(wmbus_selftest_scratch.seed)) return false;

    memcpy(wmbus_selftest_scratch.seed, test_case->vector->data, test_case->vector->len);
    wmbus_selftest_corrupt_byte(
        wmbus_selftest_scratch.seed, test_case->vector->len, test_case->seed_corrupt_byte_pos);

    if(!test_case->build_format_a) {
        *frame = wmbus_selftest_scratch.seed;
        *frame_len = test_case->vector->len;
        wmbus_selftest_apply_frame_corruption(*frame, *frame_len, test_case->frame_corrupt_byte_pos);
        return true;
    }

    if(!wmbus_frame_build_format_a(
           wmbus_selftest_scratch.seed,
           test_case->vector->len,
           wmbus_selftest_scratch.frame,
           sizeof(wmbus_selftest_scratch.frame),
           frame_len)) {
        return false;
    }

    *frame = wmbus_selftest_scratch.frame;
    wmbus_selftest_apply_frame_corruption(*frame, *frame_len, test_case->frame_corrupt_byte_pos);
    return true;
}

static bool generate_t_3of6_raw_with_offset(
    const uint8_t* decoded,
    size_t decoded_len,
    uint8_t expected_offset,
    uint8_t* out,
    size_t out_max,
    size_t* out_len,
    size_t* out_bit_len) {
    if(!decoded || !out || !out_len || !out_bit_len || expected_offset > 7) return false;

    size_t bit_pos = expected_offset;
    size_t total_bits = expected_offset + decoded_len * 12U;
    size_t raw_len = (total_bits + 7U) / 8U;
    if(raw_len > out_max) return false;

    memset(out, 0, raw_len);
    for(size_t i = 0; i < decoded_len; i++) {
        uint8_t nibbles[2] = {(uint8_t)(decoded[i] >> 4), (uint8_t)(decoded[i] & 0x0F)};
        for(size_t n = 0; n < 2; n++) {
            uint8_t symbol = wmbus_3of6_encode_lut[nibbles[n]];
            for(uint8_t bit = 0; bit < 6U; bit++) {
                size_t pos = bit_pos + bit;
                if((symbol & (1U << (5U - bit))) == 0U) continue;
                out[pos / 8U] |= (uint8_t)(1U << (7U - (pos % 8U)));
            }
            bit_pos += 6U;
        }
    }

    *out_len = raw_len;
    *out_bit_len = total_bits;
    return true;
}

static void corrupt_t_raw_bit(uint8_t* raw, size_t raw_bit_len, size_t bit_pos) {
    if(!raw || bit_pos == WMBUS_BIT_NONE) return;
    if(bit_pos >= raw_bit_len) return;
    raw[bit_pos / 8U] ^= (uint8_t)(1U << (7U - (bit_pos % 8U)));
}

static void find_best_t_offset(
    const uint8_t* raw,
    size_t raw_bit_len,
    WmBusSelftestResult* result) {
    wmbus_selftest_result_reset(result);

    int best_score = -1;
    for(uint8_t offset = 0; offset < 8U; offset++) {
        size_t decoded_len = 0;
        if(!wmbus_parser_decode_3of6_bits(
               raw,
               raw_bit_len,
               offset,
               wmbus_selftest_scratch.decoded,
               sizeof(wmbus_selftest_scratch.decoded),
               &decoded_len)) {
            continue;
        }

        WmBusSelftestResult candidate = {0};
        wmbus_selftest_analyze_decoded(
            WmBusRxModeT, wmbus_selftest_scratch.decoded, decoded_len, &candidate);
        candidate.best_offset = offset;

        int score = 0;
        if(candidate.plausible) score += 4;
        if(candidate.length_ok) score += 2;
        if(candidate.crc_ok) score += 1;

        if(score > best_score) {
            *result = candidate;
            best_score = score;
        }
    }
}

static bool wmbus_selftest_offset_match(
    const WmBusSelftestCase* test_case,
    const WmBusSelftestResult* result) {
    return (result->best_offset >= 0) &&
           ((uint8_t)result->best_offset == test_case->vector->expected_offset);
}

static const char* wmbus_selftest_format_l_field(
    const WmBusSelftestResult* result,
    char out[8]) {
    if(!result->has_l_field) {
        memcpy(out, "??", 3);
    } else {
        snprintf(out, 8, "%02X", result->l_field);
    }
    return out;
}

static const char* wmbus_selftest_format_computed_len(
    const WmBusSelftestResult* result,
    char out[16]) {
    if(!result->has_computed_len) {
        memcpy(out, "??", 3);
    } else {
        snprintf(out, 16, "%u", (unsigned int)result->computed_len);
    }
    return out;
}

static bool wmbus_selftest_run_c_case(
    const WmBusSelftestCase* test_case,
    WmBusSelftestResult* result) {
    const uint8_t* frame = NULL;
    size_t frame_len = 0;
    if(!wmbus_selftest_prepare_frame(test_case, &frame, &frame_len)) {
        wmbus_selftest_result_reset(result);
        return false;
    }

    wmbus_selftest_analyze_decoded(WmBusRxModeC, frame, frame_len, result);

    return (result->plausible == test_case->vector->expect_plausible) &&
           (result->crc_ok == test_case->vector->expect_crc_ok);
}

static bool wmbus_selftest_run_t_case(
    const WmBusSelftestCase* test_case,
    WmBusSelftestResult* result) {
    const uint8_t* frame = NULL;
    size_t frame_len = 0;
    if(!wmbus_selftest_prepare_frame(test_case, &frame, &frame_len)) {
        wmbus_selftest_result_reset(result);
        return false;
    }

    size_t raw_len = 0;
    size_t raw_bit_len = 0;
    if(!generate_t_3of6_raw_with_offset(
           frame,
           frame_len,
           test_case->vector->expected_offset,
           wmbus_selftest_scratch.raw,
           sizeof(wmbus_selftest_scratch.raw),
           &raw_len,
           &raw_bit_len)) {
        wmbus_selftest_result_reset(result);
        return false;
    }

    UNUSED(raw_len);
    corrupt_t_raw_bit(
        wmbus_selftest_scratch.raw, raw_bit_len, test_case->raw_corrupt_bit_pos);
    find_best_t_offset(wmbus_selftest_scratch.raw, raw_bit_len, result);

    bool offset_match = wmbus_selftest_offset_match(test_case, result);
    return (result->plausible == test_case->vector->expect_plausible) &&
           (result->crc_ok == test_case->vector->expect_crc_ok) &&
           (test_case->vector->expect_plausible ? offset_match : !offset_match);
}

size_t wmbus_selftest_get_case_count(void) {
    return COUNT_OF(wmbus_selftest_cases);
}

const WmBusSelftestCase* wmbus_selftest_get_case(size_t index) {
    if(index >= wmbus_selftest_get_case_count()) return NULL;
    return &wmbus_selftest_cases[index];
}

bool wmbus_selftest_run_case(const WmBusSelftestCase* test_case, WmBusSelftestResult* result) {
    WmBusSelftestResult local_result = {0};

    if(!test_case || !test_case->vector) return false;
    if(!result) result = &local_result;

    if(test_case->vector->is_t_raw) {
        return wmbus_selftest_run_t_case(test_case, result);
    } else {
        return wmbus_selftest_run_c_case(test_case, result);
    }
}

void wmbus_selftest_log_case_result(
    const WmBusSelftestCase* test_case,
    const WmBusSelftestResult* result,
    bool pass) {
    furi_check(test_case);
    furi_check(test_case->vector);
    furi_check(result);

    char l_field[8] = {0};
    char computed_len[16] = {0};
    wmbus_selftest_format_l_field(result, l_field);
    wmbus_selftest_format_computed_len(result, computed_len);

    if(test_case->vector->is_t_raw) {
        bool offset_match = wmbus_selftest_offset_match(test_case, result);
        if(pass) {
            FURI_LOG_I(
                TAG,
                "%s mode=T plausible=%s L=%s len=%s CRC=%s mfg=%s id=%s best_offset=%d expected_offset=%u expected_found=%s",
                test_case->name,
                result->plausible ? "YES" : "NO",
                l_field,
                computed_len,
                result->crc_ok ? "YES" : "NO",
                result->manufacturer,
                result->id,
                result->best_offset,
                test_case->vector->expected_offset,
                offset_match ? "YES" : "NO");
        } else {
            FURI_LOG_W(
                TAG,
                "%s mode=T plausible=%s L=%s len=%s CRC=%s mfg=%s id=%s best_offset=%d expected_offset=%u expected_found=%s",
                test_case->name,
                result->plausible ? "YES" : "NO",
                l_field,
                computed_len,
                result->crc_ok ? "YES" : "NO",
                result->manufacturer,
                result->id,
                result->best_offset,
                test_case->vector->expected_offset,
                offset_match ? "YES" : "NO");
        }
    } else {
        if(pass) {
            FURI_LOG_I(
                TAG,
                "%s mode=C plausible=%s L=%s len=%s CRC=%s mfg=%s id=%s",
                test_case->name,
                result->plausible ? "YES" : "NO",
                l_field,
                computed_len,
                result->crc_ok ? "YES" : "NO",
                result->manufacturer,
                result->id);
        } else {
            FURI_LOG_W(
                TAG,
                "%s mode=C plausible=%s L=%s len=%s CRC=%s mfg=%s id=%s",
                test_case->name,
                result->plausible ? "YES" : "NO",
                l_field,
                computed_len,
                result->crc_ok ? "YES" : "NO",
                result->manufacturer,
                result->id);
        }
    }
}

void wmbus_selftest_run_all(WmBusSelftestSummary* summary, bool log_results) {
    WmBusSelftestSummary local_summary = {0};
    if(!summary) summary = &local_summary;

    memset(summary, 0, sizeof(*summary));

    for(size_t i = 0; i < wmbus_selftest_get_case_count(); i++) {
        const WmBusSelftestCase* test_case = wmbus_selftest_get_case(i);
        WmBusSelftestResult result = {0};
        bool pass = wmbus_selftest_run_case(test_case, &result);

        summary->total++;
        if(pass) {
            summary->passed++;
        } else {
            summary->failed++;
        }

        if(log_results) {
            wmbus_selftest_log_case_result(test_case, &result, pass);
        }
    }
}

void wmbus_run_selftests(void) {
#if WMBUS_SELFTESTS
    WmBusSelftestSummary summary = {0};

    FURI_LOG_I(TAG, "selftests begin");
    wmbus_selftest_run_all(&summary, true);

    if(summary.failed == 0) {
        FURI_LOG_I(
            TAG,
            "selftests done total=%lu passed=%lu failed=0",
            summary.total,
            summary.passed);
    } else {
        FURI_LOG_W(
            TAG,
            "selftests done total=%lu passed=%lu failed=%lu",
            summary.total,
            summary.passed,
            summary.failed);
    }
#endif
}
