#include "wmbus_selftest_i.h"

#include "../protocol/decode/wmbus_decode.h"

#include <string.h>

static const WmBusTestVector wmbus_vector_c_apator_a_ok = {
    .name = "c_apator_a_ok", .data = wmbus_apator_a, .len = WMBUS_APATOR_A_LEN, .is_t_raw = false, .expect_plausible = true, .expect_crc_ok = true, .expected_offset = 0,
};
static const WmBusTestVector wmbus_vector_c_apator_b_ok = {
    .name = "c_apator_b_ok", .data = wmbus_apator_b, .len = WMBUS_APATOR_B_LEN, .is_t_raw = false, .expect_plausible = true, .expect_crc_ok = true, .expected_offset = 0,
};
static const WmBusTestVector wmbus_vector_c_apator_c_ok = {
    .name = "c_apator_c_ok", .data = wmbus_apator_c, .len = WMBUS_APATOR_C_LEN, .is_t_raw = false, .expect_plausible = true, .expect_crc_ok = true, .expected_offset = 0,
};
static const WmBusTestVector wmbus_vector_c_apator_b_crc_bad = {
    .name = "c_apator_b_crc_bad", .data = wmbus_apator_b, .len = WMBUS_APATOR_B_LEN, .is_t_raw = false, .expect_plausible = true, .expect_crc_ok = false, .expected_offset = 0,
};
static const WmBusTestVector wmbus_vector_c_apator_b_bad_c_field = {
    .name = "c_apator_b_bad_c_field", .data = wmbus_apator_b, .len = WMBUS_APATOR_B_LEN, .is_t_raw = false, .expect_plausible = false, .expect_crc_ok = false, .expected_offset = 0,
};
static const WmBusTestVector wmbus_vector_t_apator_a_off1_ok = {
    .name = "t_apator_a_off1_ok", .data = wmbus_apator_a, .len = WMBUS_APATOR_A_LEN, .is_t_raw = true, .expect_plausible = true, .expect_crc_ok = true, .expected_offset = 1,
};
static const WmBusTestVector wmbus_vector_t_apator_b_off3_ok = {
    .name = "t_apator_b_off3_ok", .data = wmbus_apator_b, .len = WMBUS_APATOR_B_LEN, .is_t_raw = true, .expect_plausible = true, .expect_crc_ok = true, .expected_offset = 3,
};
static const WmBusTestVector wmbus_vector_t_apator_c_off7_ok = {
    .name = "t_apator_c_off7_ok", .data = wmbus_apator_c, .len = WMBUS_APATOR_C_LEN, .is_t_raw = true, .expect_plausible = true, .expect_crc_ok = true, .expected_offset = 7,
};
static const WmBusTestVector wmbus_vector_t_apator_b_off5_crc_bad = {
    .name = "t_apator_b_off5_crc_bad", .data = wmbus_apator_b, .len = WMBUS_APATOR_B_LEN, .is_t_raw = true, .expect_plausible = true, .expect_crc_ok = false, .expected_offset = 5,
};
static const WmBusTestVector wmbus_vector_t_apator_b_off2_bad_symbol = {
    .name = "t_apator_b_off2_bad_symbol", .data = wmbus_apator_b, .len = WMBUS_APATOR_B_LEN, .is_t_raw = true, .expect_plausible = false, .expect_crc_ok = false, .expected_offset = 2,
};

static const WmBusSelftestCase wmbus_selftest_cases[] = {
    {.name = "c_apator_a_ok", .vector = &wmbus_vector_c_apator_a_ok, .build_format_a = true, .seed_corrupt_byte_pos = WMBUS_BIT_NONE, .frame_corrupt_byte_pos = WMBUS_BIT_NONE, .raw_corrupt_bit_pos = WMBUS_BIT_NONE},
    {.name = "c_apator_b_ok", .vector = &wmbus_vector_c_apator_b_ok, .build_format_a = true, .seed_corrupt_byte_pos = WMBUS_BIT_NONE, .frame_corrupt_byte_pos = WMBUS_BIT_NONE, .raw_corrupt_bit_pos = WMBUS_BIT_NONE},
    {.name = "c_apator_c_ok", .vector = &wmbus_vector_c_apator_c_ok, .build_format_a = true, .seed_corrupt_byte_pos = WMBUS_BIT_NONE, .frame_corrupt_byte_pos = WMBUS_BIT_NONE, .raw_corrupt_bit_pos = WMBUS_BIT_NONE},
    {.name = "c_apator_b_crc_bad", .vector = &wmbus_vector_c_apator_b_crc_bad, .build_format_a = true, .seed_corrupt_byte_pos = WMBUS_BIT_NONE, .frame_corrupt_byte_pos = WMBUS_BYTE_LAST, .raw_corrupt_bit_pos = WMBUS_BIT_NONE},
    {.name = "c_apator_b_bad_c_field", .vector = &wmbus_vector_c_apator_b_bad_c_field, .build_format_a = true, .seed_corrupt_byte_pos = 1U, .frame_corrupt_byte_pos = WMBUS_BIT_NONE, .raw_corrupt_bit_pos = WMBUS_BIT_NONE},
    {.name = "t_apator_a_off1_ok", .vector = &wmbus_vector_t_apator_a_off1_ok, .build_format_a = true, .seed_corrupt_byte_pos = WMBUS_BIT_NONE, .frame_corrupt_byte_pos = WMBUS_BIT_NONE, .raw_corrupt_bit_pos = WMBUS_BIT_NONE},
    {.name = "t_apator_b_off3_ok", .vector = &wmbus_vector_t_apator_b_off3_ok, .build_format_a = true, .seed_corrupt_byte_pos = WMBUS_BIT_NONE, .frame_corrupt_byte_pos = WMBUS_BIT_NONE, .raw_corrupt_bit_pos = WMBUS_BIT_NONE},
    {.name = "t_apator_c_off7_ok", .vector = &wmbus_vector_t_apator_c_off7_ok, .build_format_a = true, .seed_corrupt_byte_pos = WMBUS_BIT_NONE, .frame_corrupt_byte_pos = WMBUS_BIT_NONE, .raw_corrupt_bit_pos = WMBUS_BIT_NONE},
    {.name = "t_apator_b_off5_crc_bad", .vector = &wmbus_vector_t_apator_b_off5_crc_bad, .build_format_a = true, .seed_corrupt_byte_pos = WMBUS_BIT_NONE, .frame_corrupt_byte_pos = WMBUS_BYTE_LAST, .raw_corrupt_bit_pos = WMBUS_BIT_NONE},
    {.name = "t_apator_b_off2_bad_symbol", .vector = &wmbus_vector_t_apator_b_off2_bad_symbol, .build_format_a = true, .seed_corrupt_byte_pos = WMBUS_BIT_NONE, .frame_corrupt_byte_pos = WMBUS_BIT_NONE, .raw_corrupt_bit_pos = wmbus_vector_t_apator_b_off2_bad_symbol.expected_offset},
};

static bool wmbus_selftest_offset_match(
    const WmBusSelftestCase* test_case,
    const WmBusSelftestResult* result) {
    return (result->best_offset >= 0) &&
           ((uint8_t)result->best_offset == test_case->vector->expected_offset);
}

static bool
    wmbus_selftest_run_c_case(const WmBusSelftestCase* test_case, WmBusSelftestResult* result) {
    const uint8_t* frame = NULL;
    size_t frame_len = 0;
    if(!wmbus_selftest_prepare_frame(test_case, &frame, &frame_len)) {
        wmbus_selftest_result_reset(result);
        return false;
    }

    if(!wmbus_selftest_run_capture(WmBusRxModeC, frame, frame_len, NULL, result)) return false;

    return (result->plausible == test_case->vector->expect_plausible) &&
           (result->crc_ok == test_case->vector->expect_crc_ok);
}

static bool
    wmbus_selftest_run_t_case(const WmBusSelftestCase* test_case, WmBusSelftestResult* result) {
    const uint8_t* frame = NULL;
    size_t frame_len = 0;
    uint8_t raw[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t raw_len = 0;
    size_t raw_bit_len = 0;

    if(!wmbus_selftest_prepare_frame(test_case, &frame, &frame_len)) {
        wmbus_selftest_result_reset(result);
        return false;
    }

    if(!wmbus_selftest_generate_t_3of6_raw_with_offset(
           frame,
           frame_len,
           test_case->vector->expected_offset,
           raw,
           sizeof(raw),
           &raw_len,
           &raw_bit_len)) {
        wmbus_selftest_result_reset(result);
        return false;
    }

    wmbus_selftest_corrupt_t_raw_bit(raw, raw_bit_len, test_case->raw_corrupt_bit_pos);
    if(!wmbus_selftest_run_capture(WmBusRxModeT, raw, raw_len, NULL, result)) return false;

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

    return test_case->vector->is_t_raw ? wmbus_selftest_run_t_case(test_case, result) :
                                         wmbus_selftest_run_c_case(test_case, result);
}

void wmbus_selftest_log_case_result(
    const WmBusSelftestCase* test_case,
    const WmBusSelftestResult* result,
    bool pass) {
    char l_field[8] = {0};
    char computed_len[16] = {0};
    wmbus_selftest_format_l_field(result, l_field);
    wmbus_selftest_format_computed_len(result, computed_len);

    if(test_case->vector->is_t_raw) {
        bool offset_match = wmbus_selftest_offset_match(test_case, result);
        if(pass) {
            FURI_LOG_I(TAG, "%s mode=T plausible=%s L=%s len=%s CRC=%s mfg=%s id=%s best_offset=%d expected_offset=%u expected_found=%s", test_case->name, result->plausible ? "YES" : "NO", l_field, computed_len, result->crc_ok ? "YES" : "NO", result->manufacturer, result->id, result->best_offset, test_case->vector->expected_offset, offset_match ? "YES" : "NO");
        } else {
            FURI_LOG_W(TAG, "%s mode=T plausible=%s L=%s len=%s CRC=%s mfg=%s id=%s best_offset=%d expected_offset=%u expected_found=%s", test_case->name, result->plausible ? "YES" : "NO", l_field, computed_len, result->crc_ok ? "YES" : "NO", result->manufacturer, result->id, result->best_offset, test_case->vector->expected_offset, offset_match ? "YES" : "NO");
        }
    } else {
        if(pass) {
            FURI_LOG_I(TAG, "%s mode=C plausible=%s L=%s len=%s CRC=%s mfg=%s id=%s", test_case->name, result->plausible ? "YES" : "NO", l_field, computed_len, result->crc_ok ? "YES" : "NO", result->manufacturer, result->id);
        } else {
            FURI_LOG_W(TAG, "%s mode=C plausible=%s L=%s len=%s CRC=%s mfg=%s id=%s", test_case->name, result->plausible ? "YES" : "NO", l_field, computed_len, result->crc_ok ? "YES" : "NO", result->manufacturer, result->id);
        }
    }
}

void wmbus_selftest_report_case_result(
    File* file,
    const WmBusSelftestCase* test_case,
    const WmBusSelftestResult* result,
    bool pass) {
    char l_field[8] = {0};
    char computed_len[16] = {0};
    wmbus_selftest_format_l_field(result, l_field);
    wmbus_selftest_format_computed_len(result, computed_len);

    if(test_case->vector->is_t_raw) {
        bool offset_match = wmbus_selftest_offset_match(test_case, result);
        wmbus_selftest_write_report_line(
            file,
            "%s %s mode=T plausible=%s L=%s len=%s CRC=%s mfg=%s id=%s best_offset=%d expected_offset=%u expected_found=%s\n",
            pass ? "PASS" : "FAIL",
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
        wmbus_selftest_write_report_line(
            file,
            "%s %s mode=C plausible=%s L=%s len=%s CRC=%s mfg=%s id=%s\n",
            pass ? "PASS" : "FAIL",
            test_case->name,
            result->plausible ? "YES" : "NO",
            l_field,
            computed_len,
            result->crc_ok ? "YES" : "NO",
            result->manufacturer,
            result->id);
    }
}

static bool wmbus_selftest_check_capture_reconstruct_c_frame(char* detail, size_t detail_len) {
    const uint8_t payload[] = {0x44, 0x01, 0x06, 0x20, 0x20};
    uint8_t frame[16] = {0};
    size_t frame_len = 0;

    if(!wmbus_capture_reconstruct_c_frame(payload, sizeof(payload), frame, sizeof(frame), &frame_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "reconstruct failed");
        return false;
    }
    if(frame_len != sizeof(payload) + 1U || frame[0] != sizeof(payload) || frame[1] != 0x44U ||
       frame[4] != 0x20U) {
        wmbus_selftest_set_detail(detail, detail_len, "unexpected frame_len=%u L=%02X C=%02X id0=%02X", (unsigned int)frame_len, frame[0], frame[1], frame[4]);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "frame_len=%u L=%02X C=%02X", 6U, frame[0], frame[1]);
    return true;
}

static bool wmbus_selftest_check_capture_reconstruct_c_frame_reject_oversize(
    char* detail,
    size_t detail_len) {
    uint8_t payload[256] = {0};
    uint8_t frame[300] = {0};
    size_t frame_len = 0;

    if(wmbus_capture_reconstruct_c_frame(payload, sizeof(payload), frame, sizeof(frame), &frame_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "oversize payload accepted");
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "reject=YES");
    return true;
}

static bool wmbus_selftest_check_capture_t_expected_raw_len_estimate(char* detail, size_t detail_len) {
    const uint8_t raw[] = {0x5A, 0x60};
    size_t expected_raw_len = 0;
    if(!wmbus_capture_estimate_t_expected_raw_len(raw, sizeof(raw), 256U, &expected_raw_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "estimate failed");
        return false;
    }
    if(expected_raw_len != 23U) {
        wmbus_selftest_set_detail(detail, detail_len, "unexpected expected_raw_len=%u", (unsigned int)expected_raw_len);
        return false;
    }
    wmbus_selftest_set_detail(detail, detail_len, "expected_raw_len=23");
    return true;
}

static bool wmbus_selftest_check_capture_t_expected_raw_len_clamp(char* detail, size_t detail_len) {
    const uint8_t raw[] = {0x5A, 0x60};
    size_t expected_raw_len = 0;
    if(!wmbus_capture_estimate_t_expected_raw_len(raw, sizeof(raw), 20U, &expected_raw_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "estimate failed");
        return false;
    }
    if(expected_raw_len != 20U) {
        wmbus_selftest_set_detail(detail, detail_len, "unexpected expected_raw_len=%u", (unsigned int)expected_raw_len);
        return false;
    }
    wmbus_selftest_set_detail(detail, detail_len, "expected_raw_len=20");
    return true;
}

static bool wmbus_selftest_check_capture_t_expected_raw_len_short_is_tentative(char* detail, size_t detail_len) {
    const uint8_t raw[] = {0x5A, 0x60};
    size_t expected_raw_len = 0;
    int expected_score = 0;
    if(!wmbus_capture_estimate_t_expected_raw_len_scored(
           raw, sizeof(raw), 256U, &expected_raw_len, &expected_score)) {
        wmbus_selftest_set_detail(detail, detail_len, "estimate failed");
        return false;
    }
    if(expected_raw_len != 23U || expected_score != 1) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "unexpected expected_raw_len=%u score=%d",
            (unsigned int)expected_raw_len,
            expected_score);
        return false;
    }
    wmbus_selftest_set_detail(detail, detail_len, "expected_raw_len=23 score=1");
    return true;
}

static bool wmbus_selftest_check_capture_t_expected_raw_len_full_is_confident(char* detail, size_t detail_len) {
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t frame_len = 0;
    uint8_t raw[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t raw_len = 0;
    size_t raw_bit_len = 0;
    size_t expected_raw_len = 0;
    int expected_score = 0;

    if(!wmbus_frame_build_format_a(wmbus_apator_b, WMBUS_APATOR_B_LEN, frame, sizeof(frame), &frame_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "build format-A failed");
        return false;
    }
    if(!wmbus_selftest_generate_t_3of6_raw_with_offset(
           frame, frame_len, 3U, raw, sizeof(raw), &raw_len, &raw_bit_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "generate raw failed");
        return false;
    }
    if(!wmbus_capture_estimate_t_expected_raw_len_scored(
           raw, raw_len, sizeof(raw), &expected_raw_len, &expected_score)) {
        wmbus_selftest_set_detail(detail, detail_len, "estimate failed");
        return false;
    }
    if(expected_score < 3 || expected_raw_len != raw_len) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "unexpected expected_raw_len=%u raw_len=%u score=%d",
            (unsigned int)expected_raw_len,
            (unsigned int)raw_len,
            expected_score);
        return false;
    }
    wmbus_selftest_set_detail(
        detail, detail_len, "expected_raw_len=%u score=%d", (unsigned int)expected_raw_len, expected_score);
    return true;
}

static bool wmbus_selftest_check_packet_process_t_ignores_invalid_tail(
    char* detail,
    size_t detail_len,
    size_t tail_len) {
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t frame_len = 0;
    uint8_t raw[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t raw_len = 0;
    size_t raw_bit_len = 0;
    uint8_t capture[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t expected_raw_len = 0;
    int expected_score = 0;
    WmBusPacketRecord record = {0};

    if(!wmbus_frame_build_format_a(wmbus_apator_b, WMBUS_APATOR_B_LEN, frame, sizeof(frame), &frame_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "build format-A failed");
        return false;
    }
    if(!wmbus_selftest_generate_t_3of6_raw_with_offset(
           frame, frame_len, 3U, raw, sizeof(raw), &raw_len, &raw_bit_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "generate raw failed");
        return false;
    }
    if(raw_len + tail_len > sizeof(capture)) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "capture overflow raw_len=%u tail=%u",
            (unsigned int)raw_len,
            (unsigned int)tail_len);
        return false;
    }

    memcpy(capture, raw, raw_len);
    memset(&capture[raw_len], 0x00, tail_len);

    if(!wmbus_capture_estimate_t_expected_raw_len_scored(
           capture, raw_len + tail_len, sizeof(capture), &expected_raw_len, &expected_score)) {
        wmbus_selftest_set_detail(detail, detail_len, "estimate failed");
        return false;
    }
    if(expected_raw_len != raw_len || expected_score < 3) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "unexpected estimate expected=%u raw=%u score=%d",
            (unsigned int)expected_raw_len,
            (unsigned int)raw_len,
            expected_score);
        return false;
    }

    if(!wmbus_selftest_process_capture_record(
           WmBusRxModeT, capture, raw_len + tail_len, NULL, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "process failed");
        return false;
    }
    if(!wmbus_packet_quality_meets(record.quality, WmBusPacketQualityCrcOk) ||
       record.best_offset != 3 || strcmp(record.identity.meter_id, "21202020") != 0) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "unexpected quality=%u offset=%d id=%s",
            (unsigned int)record.quality,
            record.best_offset,
            record.identity.meter_id);
        return false;
    }

    wmbus_selftest_set_detail(
        detail,
        detail_len,
        "tail=%u expected_raw_len=%u score=%d",
        (unsigned int)tail_len,
        (unsigned int)expected_raw_len,
        expected_score);
    return true;
}

static bool wmbus_selftest_check_packet_process_t_ignores_invalid_tail_1(char* detail, size_t detail_len) {
    return wmbus_selftest_check_packet_process_t_ignores_invalid_tail(detail, detail_len, 1U);
}

static bool wmbus_selftest_check_packet_process_t_ignores_invalid_tail_16(char* detail, size_t detail_len) {
    return wmbus_selftest_check_packet_process_t_ignores_invalid_tail(detail, detail_len, 16U);
}

static bool wmbus_selftest_check_packet_process_t_ignores_invalid_tail_64(char* detail, size_t detail_len) {
    return wmbus_selftest_check_packet_process_t_ignores_invalid_tail(detail, detail_len, 64U);
}

static bool wmbus_selftest_check_capture_t_expected_raw_len_reject_invalid(char* detail, size_t detail_len) {
    const uint8_t raw[] = {0x00, 0x00};
    size_t expected_raw_len = 0;
    if(wmbus_capture_estimate_t_expected_raw_len(raw, sizeof(raw), 256U, &expected_raw_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "invalid raw accepted len=%u", (unsigned int)expected_raw_len);
        return false;
    }
    wmbus_selftest_set_detail(detail, detail_len, "reject=YES");
    return true;
}

static bool wmbus_selftest_check_packet_process_t_sync_after_fifo_prefix(
    char* detail,
    size_t detail_len) {
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t frame_len = 0;
    uint8_t raw[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t raw_len = 0;
    size_t raw_bit_len = 0;
    uint8_t capture[WMBUS_SELFTEST_BUF_MAX] = {0};
    WmBusPacketRecord record = {0};

    if(!wmbus_frame_build_format_a(wmbus_apator_b, WMBUS_APATOR_B_LEN, frame, sizeof(frame), &frame_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "build format-A failed");
        return false;
    }
    if(!wmbus_selftest_generate_t_3of6_raw_with_offset(
           frame, frame_len, 0U, raw, sizeof(raw), &raw_len, &raw_bit_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "generate raw failed");
        return false;
    }
    if(raw_len + 2U > sizeof(capture)) {
        wmbus_selftest_set_detail(detail, detail_len, "capture overflow raw_len=%u", (unsigned int)raw_len);
        return false;
    }

    capture[0] = 0x3CU;
    capture[1] = 0x94U;
    memcpy(&capture[2], raw, raw_len);

    if(!wmbus_selftest_process_capture_record(WmBusRxModeT, capture, raw_len + 2U, NULL, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "process failed");
        return false;
    }
    if(!wmbus_packet_quality_meets(record.quality, WmBusPacketQualityCrcOk) ||
       record.best_offset != 16 ||
       strcmp(record.identity.meter_id, "21202020") != 0) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "unexpected quality=%u best_offset=%d id=%s",
            (unsigned int)record.quality,
            record.best_offset,
            record.identity.meter_id);
        return false;
    }

    wmbus_selftest_set_detail(
        detail,
        detail_len,
        "fifo_prefix=3C94 best_offset=16 id=%s",
        record.identity.meter_id);
    return true;
}

static bool wmbus_selftest_check_capture_c_frame_offset_waits_for_disambiguation(char* detail, size_t detail_len) {
    const uint8_t raw[] = {0x54, 0x3E};
    size_t frame_offset = wmbus_capture_c_frame_offset(raw, sizeof(raw));
    if(frame_offset != SIZE_MAX) {
        wmbus_selftest_set_detail(detail, detail_len, "unexpected frame_offset=%u", (unsigned int)frame_offset);
        return false;
    }
    wmbus_selftest_set_detail(detail, detail_len, "offset=WAIT");
    return true;
}

static bool wmbus_selftest_check_capture_c_frame_offset_rejects_leading_sync_byte(
    char* detail,
    size_t detail_len) {
    const uint8_t raw[] = {0x54, 0x3E, 0x44, 0x01, 0x06};
    size_t frame_offset = wmbus_capture_c_frame_offset(raw, sizeof(raw));
    if(frame_offset != SIZE_MAX) {
        wmbus_selftest_set_detail(detail, detail_len, "unexpected frame_offset=%u", (unsigned int)frame_offset);
        return false;
    }
    wmbus_selftest_set_detail(detail, detail_len, "leading_sync=REJECT");
    return true;
}

static bool wmbus_selftest_check_capture_c_frame_offset_l_field_54(char* detail, size_t detail_len) {
    const uint8_t raw[] = {0x54, 0x44, 0x01, 0x06, 0x20};
    size_t frame_offset = wmbus_capture_c_frame_offset(raw, sizeof(raw));
    if(frame_offset != 0U) {
        wmbus_selftest_set_detail(detail, detail_len, "unexpected frame_offset=%u", (unsigned int)frame_offset);
        return false;
    }
    wmbus_selftest_set_detail(detail, detail_len, "frame_offset=0");
    return true;
}

static bool wmbus_selftest_check_capture_c_accepts_access_demand(char* detail, size_t detail_len) {
    const uint8_t raw[] = {0x0A, 0x48, 0x01, 0x06, 0x20, 0x20, 0x20, 0x20, 0x05, 0x07, 0x7A};
    size_t frame_offset = wmbus_capture_c_frame_offset(raw, sizeof(raw));
    if(frame_offset != 0U || !wmbus_decode_is_plausible_frame(raw, sizeof(raw))) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "access demand rejected offset=%u",
            (unsigned int)frame_offset);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "c_field=48 accepted");
    return true;
}

static bool wmbus_selftest_check_capture_c_select_rejects_timeout_noise(
    char* detail,
    size_t detail_len) {
    const uint8_t raw[32] = {
        0x8B, 0x12, 0xA4, 0x39, 0x71, 0x00, 0xE3, 0x5C,
        0x9D, 0x28, 0xB6, 0x41, 0x0F, 0xD2, 0x63, 0x88,
        0x13, 0x7A, 0xC1, 0x2E, 0x55, 0x99, 0x04, 0xF0,
        0xAA, 0x6D, 0x31, 0xC7, 0x18, 0x82, 0x4B, 0xDE};
    size_t frame_offset = 0U;
    size_t frame_len = 0U;

    if(wmbus_capture_select_c_frame(raw, sizeof(raw), 0U, &frame_offset, &frame_len)) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "timeout noise selected offset=%u len=%u",
            (unsigned int)frame_offset,
            (unsigned int)frame_len);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "timeout_noise=REJECT");
    return true;
}

static bool wmbus_selftest_check_capture_c_select_rejects_leading_sync_byte(
    char* detail,
    size_t detail_len) {
    const uint8_t raw[] = {0x54, 0x3E, 0x44, 0x01, 0x06};
    size_t frame_offset = 0U;
    size_t frame_len = 0U;

    if(wmbus_capture_select_c_frame(raw, sizeof(raw), 0U, &frame_offset, &frame_len)) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "leading sync selected offset=%u len=%u",
            (unsigned int)frame_offset,
            (unsigned int)frame_len);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "leading_sync=REJECT");
    return true;
}

static bool wmbus_selftest_check_packet_process_c_bad_header_keeps_raw_diagnostic(
    char* detail,
    size_t detail_len) {
    const uint8_t raw[] = {0x24, 0x99, 0x00, 0x00, 0xA5, 0x5A};
    WmBusPacketRecord record = {0};

    if(!wmbus_selftest_process_capture_record(WmBusRxModeC, raw, sizeof(raw), NULL, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "process failed");
        return false;
    }

    if(record.quality != WmBusPacketQualityAnyCapture || record.packet_len != sizeof(raw) ||
       memcmp(record.packet_bytes, raw, sizeof(raw)) != 0) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "unexpected quality=%u packet_len=%u",
            (unsigned int)record.quality,
            (unsigned int)record.packet_len);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "quality=Any capture raw diagnostic kept");
    return true;
}

static bool wmbus_selftest_check_capture_c_expected_len_estimate(char* detail, size_t detail_len) {
    const uint8_t raw[] = {0x3E, 0x44, 0x01, 0x06};
    size_t expected_len = 0;
    if(!wmbus_capture_estimate_c_expected_len(raw, sizeof(raw), 256U, &expected_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "estimate failed");
        return false;
    }
    if(expected_len != 73U) {
        wmbus_selftest_set_detail(detail, detail_len, "unexpected expected_len=%u", (unsigned int)expected_len);
        return false;
    }
    wmbus_selftest_set_detail(detail, detail_len, "expected_len=73");
    return true;
}

static bool wmbus_selftest_check_capture_c_expected_len_rejects_leading_sync_byte(
    char* detail,
    size_t detail_len) {
    const uint8_t raw[] = {0x54, 0x3E, 0x44, 0x01, 0x06};
    size_t expected_len = 0;
    if(wmbus_capture_estimate_c_expected_len(raw, sizeof(raw), 256U, &expected_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "unexpected expected_len=%u", (unsigned int)expected_len);
        return false;
    }
    wmbus_selftest_set_detail(detail, detail_len, "leading_sync=REJECT");
    return true;
}

static bool wmbus_selftest_check_capture_c_expected_len_real_l_field_54(char* detail, size_t detail_len) {
    const uint8_t raw[] = {0x54, 0x44, 0x01, 0x06, 0x20};
    size_t expected_len = 0;
    if(!wmbus_capture_estimate_c_expected_len(raw, sizeof(raw), 256U, &expected_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "estimate failed for L=54");
        return false;
    }
    if(expected_len != 97U) {
        wmbus_selftest_set_detail(detail, detail_len, "unexpected expected_len=%u", (unsigned int)expected_len);
        return false;
    }
    wmbus_selftest_set_detail(detail, detail_len, "expected_len=97");
    return true;
}

static bool
    wmbus_selftest_check_capture_c_expected_len_estimate_format_b_crc_match(
        char* detail,
        size_t detail_len) {
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t frame_len = 0;
    size_t expected_len = 0;

    if(!wmbus_frame_build_format_b(wmbus_apator_c, WMBUS_APATOR_C_LEN, frame, sizeof(frame), &frame_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "build format-B failed");
        return false;
    }
    if(!wmbus_capture_estimate_c_expected_len(frame, frame_len, sizeof(frame), &expected_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "estimate failed");
        return false;
    }
    if(expected_len != frame_len) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "unexpected expected_len=%u frame_len=%u",
            (unsigned int)expected_len,
            (unsigned int)frame_len);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "expected_len=%u", (unsigned int)frame_len);
    return true;
}

static bool wmbus_selftest_check_frame_normalize_format_a_wire_frame(char* detail, size_t detail_len) {
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t frame_len = 0;
    uint8_t normalized[WMBUS_SELFTEST_BUF_MAX] = {0};
    WmBusFrameNormalizeResult result = {0};

    if(!wmbus_frame_build_format_a(wmbus_apator_a, WMBUS_APATOR_A_LEN, frame, sizeof(frame), &frame_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "build format-A failed");
        return false;
    }
    if(!wmbus_frame_normalize(WmBusRxModeT, frame, frame_len, normalized, sizeof(normalized), &result)) {
        wmbus_selftest_set_detail(detail, detail_len, "normalize format-A failed");
        return false;
    }
    if(!result.length_ok || !result.crc_known || !result.crc_ok || result.format != WmBusFrameFormatA ||
       result.normalized_len != WMBUS_APATOR_A_LEN ||
       memcmp(wmbus_apator_a, normalized, WMBUS_APATOR_A_LEN) != 0) {
        wmbus_selftest_set_detail(detail, detail_len, "unexpected format=%u len_ok=%u crc_ok=%u normalized_len=%u", (unsigned int)result.format, result.length_ok ? 1U : 0U, result.crc_ok ? 1U : 0U, (unsigned int)result.normalized_len);
        return false;
    }
    wmbus_selftest_set_detail(detail, detail_len, "format=A normalized_len=%u", 111U);
    return true;
}

static bool wmbus_selftest_check_frame_normalize_c_mode_format_a_wire_frame(char* detail, size_t detail_len) {
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t frame_len = 0;
    uint8_t normalized[WMBUS_SELFTEST_BUF_MAX] = {0};
    WmBusFrameNormalizeResult result = {0};

    if(!wmbus_frame_build_format_a(wmbus_apator_b, WMBUS_APATOR_B_LEN, frame, sizeof(frame), &frame_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "build format-A failed");
        return false;
    }
    if(!wmbus_frame_normalize(WmBusRxModeC, frame, frame_len, normalized, sizeof(normalized), &result)) {
        wmbus_selftest_set_detail(detail, detail_len, "normalize format-A failed");
        return false;
    }
    if(!result.length_ok || !result.crc_known || !result.crc_ok || result.format != WmBusFrameFormatA ||
       result.normalized_len != WMBUS_APATOR_B_LEN ||
       memcmp(wmbus_apator_b, normalized, WMBUS_APATOR_B_LEN) != 0) {
        wmbus_selftest_set_detail(detail, detail_len, "unexpected format=%u len_ok=%u crc_ok=%u normalized_len=%u", (unsigned int)result.format, result.length_ok ? 1U : 0U, result.crc_ok ? 1U : 0U, (unsigned int)result.normalized_len);
        return false;
    }
    wmbus_selftest_set_detail(detail, detail_len, "mode=C format=A normalized_len=%u", 79U);
    return true;
}

static bool wmbus_selftest_check_frame_normalize_format_b_wire_frame(char* detail, size_t detail_len) {
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t frame_len = 0;
    uint8_t normalized[WMBUS_SELFTEST_BUF_MAX] = {0};
    WmBusFrameNormalizeResult result = {0};

    if(!wmbus_frame_build_format_b(wmbus_apator_c, WMBUS_APATOR_C_LEN, frame, sizeof(frame), &frame_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "build format-B failed");
        return false;
    }
    if(!wmbus_frame_normalize(WmBusRxModeC, frame, frame_len, normalized, sizeof(normalized), &result)) {
        wmbus_selftest_set_detail(detail, detail_len, "normalize format-B failed");
        return false;
    }
    if(!result.length_ok || !result.crc_known || !result.crc_ok || result.format != WmBusFrameFormatB ||
       result.normalized_len != WMBUS_APATOR_C_LEN ||
       memcmp(wmbus_apator_c, normalized, WMBUS_APATOR_C_LEN) != 0) {
        wmbus_selftest_set_detail(detail, detail_len, "unexpected format=%u len_ok=%u crc_ok=%u normalized_len=%u", (unsigned int)result.format, result.length_ok ? 1U : 0U, result.crc_ok ? 1U : 0U, (unsigned int)result.normalized_len);
        return false;
    }
    wmbus_selftest_set_detail(detail, detail_len, "format=B normalized_len=%u", 63U);
    return true;
}

static bool wmbus_selftest_check_packet_process_c_crc_bad_keeps_raw_diagnostic(
    char* detail,
    size_t detail_len) {
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t frame_len = 0;
    WmBusPacketRecord record = {0};

    if(!wmbus_frame_build_format_a(wmbus_apator_b, WMBUS_APATOR_B_LEN, frame, sizeof(frame), &frame_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "build format-A failed");
        return false;
    }

    frame[5] ^= 0x01U;

    if(wmbus_frame_crc_check(WmBusFrameFormatA, frame, frame_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "corrupt frame unexpectedly passed CRC");
        return false;
    }

    uint8_t normalized[WMBUS_SELFTEST_BUF_MAX] = {0};
    WmBusFrameNormalizeResult normalize = {0};
    if(wmbus_frame_normalize(WmBusRxModeC, frame, frame_len, normalized, sizeof(normalized), &normalize) ||
       !normalize.crc_known || normalize.crc_ok || normalize.length_ok ||
       normalize.format != WmBusFrameFormatA || normalize.computed_len != frame_len ||
       normalize.normalized_len != WMBUS_APATOR_B_LEN) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "unexpected normalize crc_known=%u crc_ok=%u len_ok=%u format=%u computed=%u norm=%u",
            normalize.crc_known ? 1U : 0U,
            normalize.crc_ok ? 1U : 0U,
            normalize.length_ok ? 1U : 0U,
            (unsigned int)normalize.format,
            (unsigned int)normalize.computed_len,
            (unsigned int)normalize.normalized_len);
        return false;
    }

    if(!wmbus_selftest_process_capture_record(WmBusRxModeC, frame, frame_len, NULL, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "process failed");
        return false;
    }

    if(record.quality != WmBusPacketQualityHeaderOk || record.packet_len != frame_len ||
       memcmp(record.packet_bytes, frame, frame_len) != 0 || record.application.parser_id != WmBusParserIdRaw) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "unexpected quality=%u packet_len=%u parser=%u",
            (unsigned int)record.quality,
            (unsigned int)record.packet_len,
            (unsigned int)record.application.parser_id);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "mode=C crc_bad raw diagnostic kept");
    return true;
}

static bool wmbus_selftest_check_capture_state_reset(char* detail, size_t detail_len) {
    WmBusCaptureStateT state_t = {.raw_len = 9U, .in_packet = true, .expected_raw_len = 42U, .expected_raw_score = 3, .last_byte_tick = 1234U};
    WmBusCaptureStateC state_c = {.raw_len = 12U, .in_packet = true, .expected_len = 73U, .last_byte_tick = 5678U};

    wmbus_capture_state_t_reset(&state_t);
    wmbus_capture_state_c_reset(&state_c);

    if(state_t.raw_len != 0U || state_t.in_packet || state_t.expected_raw_len != 0U ||
       state_t.expected_raw_score != 0 || state_t.last_byte_tick != 0U) {
        wmbus_selftest_set_detail(detail, detail_len, "T state reset failed");
        return false;
    }
    if(state_c.raw_len != 0U || state_c.in_packet || state_c.expected_len != 0U || state_c.last_byte_tick != 0U) {
        wmbus_selftest_set_detail(detail, detail_len, "C state reset failed");
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "state_reset=YES");
    return true;
}

static const WmBusSelftestCheck wmbus_selftest_checks_modes[] = {
    {"check_capture_reconstruct_c_frame", wmbus_selftest_check_capture_reconstruct_c_frame},
    {"check_capture_reconstruct_c_frame_reject_oversize", wmbus_selftest_check_capture_reconstruct_c_frame_reject_oversize},
    {"check_capture_t_expected_raw_len_estimate", wmbus_selftest_check_capture_t_expected_raw_len_estimate},
    {"check_capture_t_expected_raw_len_clamp", wmbus_selftest_check_capture_t_expected_raw_len_clamp},
    {"check_capture_t_expected_raw_len_short_is_tentative", wmbus_selftest_check_capture_t_expected_raw_len_short_is_tentative},
    {"check_capture_t_expected_raw_len_full_is_confident", wmbus_selftest_check_capture_t_expected_raw_len_full_is_confident},
    {"check_packet_process_t_ignores_invalid_tail_1", wmbus_selftest_check_packet_process_t_ignores_invalid_tail_1},
    {"check_packet_process_t_ignores_invalid_tail_16", wmbus_selftest_check_packet_process_t_ignores_invalid_tail_16},
    {"check_packet_process_t_ignores_invalid_tail_64", wmbus_selftest_check_packet_process_t_ignores_invalid_tail_64},
    {"check_capture_t_expected_raw_len_reject_invalid", wmbus_selftest_check_capture_t_expected_raw_len_reject_invalid},
    {"check_packet_process_t_sync_after_fifo_prefix", wmbus_selftest_check_packet_process_t_sync_after_fifo_prefix},
    {"check_capture_c_frame_offset_waits_for_disambiguation", wmbus_selftest_check_capture_c_frame_offset_waits_for_disambiguation},
    {"check_capture_c_frame_offset_rejects_leading_sync_byte", wmbus_selftest_check_capture_c_frame_offset_rejects_leading_sync_byte},
    {"check_capture_c_frame_offset_l_field_54", wmbus_selftest_check_capture_c_frame_offset_l_field_54},
    {"check_capture_c_accepts_access_demand", wmbus_selftest_check_capture_c_accepts_access_demand},
    {"check_capture_c_select_rejects_timeout_noise", wmbus_selftest_check_capture_c_select_rejects_timeout_noise},
    {"check_capture_c_select_rejects_leading_sync_byte", wmbus_selftest_check_capture_c_select_rejects_leading_sync_byte},
    {"check_packet_process_c_bad_header_keeps_raw_diagnostic", wmbus_selftest_check_packet_process_c_bad_header_keeps_raw_diagnostic},
    {"check_capture_c_expected_len_estimate", wmbus_selftest_check_capture_c_expected_len_estimate},
    {"check_capture_c_expected_len_rejects_leading_sync_byte", wmbus_selftest_check_capture_c_expected_len_rejects_leading_sync_byte},
    {"check_capture_c_expected_len_real_l_field_54", wmbus_selftest_check_capture_c_expected_len_real_l_field_54},
    {"check_capture_c_expected_len_estimate_format_b_crc_match", wmbus_selftest_check_capture_c_expected_len_estimate_format_b_crc_match},
    {"check_frame_normalize_format_a_wire_frame", wmbus_selftest_check_frame_normalize_format_a_wire_frame},
    {"check_frame_normalize_c_mode_format_a_wire_frame", wmbus_selftest_check_frame_normalize_c_mode_format_a_wire_frame},
    {"check_frame_normalize_format_b_wire_frame", wmbus_selftest_check_frame_normalize_format_b_wire_frame},
    {"check_packet_process_c_crc_bad_keeps_raw_diagnostic", wmbus_selftest_check_packet_process_c_crc_bad_keeps_raw_diagnostic},
    {"check_capture_state_reset", wmbus_selftest_check_capture_state_reset},
};

const WmBusSelftestCheck* wmbus_selftest_mode_checks(size_t* count) {
    if(count) *count = COUNT_OF(wmbus_selftest_checks_modes);
    return wmbus_selftest_checks_modes;
}
