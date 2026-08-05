#include "wmbus_selftest_i.h"

#include "../protocol/decode/wmbus_decode.h"

#include <string.h>

static const WmBusTestVector wmbus_vector_c_apator_a_ok = {
    .name = "c_apator_a_ok",
    .data = wmbus_apator_a,
    .len = WMBUS_APATOR_A_LEN,
    .is_t_raw = false,
    .expect_plausible = true,
    .expect_crc_ok = true,
};
static const WmBusTestVector wmbus_vector_c_apator_b_ok = {
    .name = "c_apator_b_ok",
    .data = wmbus_apator_b,
    .len = WMBUS_APATOR_B_LEN,
    .is_t_raw = false,
    .expect_plausible = true,
    .expect_crc_ok = true,
};
static const WmBusTestVector wmbus_vector_c_apator_c_ok = {
    .name = "c_apator_c_ok",
    .data = wmbus_apator_c,
    .len = WMBUS_APATOR_C_LEN,
    .is_t_raw = false,
    .expect_plausible = true,
    .expect_crc_ok = true,
};
static const WmBusTestVector wmbus_vector_c_apator_b_crc_bad = {
    .name = "c_apator_b_crc_bad",
    .data = wmbus_apator_b,
    .len = WMBUS_APATOR_B_LEN,
    .is_t_raw = false,
    .expect_plausible = true,
    .expect_crc_ok = false,
};
static const WmBusTestVector wmbus_vector_c_apator_b_bad_c_field = {
    .name = "c_apator_b_bad_c_field",
    .data = wmbus_apator_b,
    .len = WMBUS_APATOR_B_LEN,
    .is_t_raw = false,
    .expect_plausible = false,
    .expect_crc_ok = false,
};
static const WmBusTestVector wmbus_vector_t_apator_a_ok = {
    .name = "t_apator_a_ok",
    .data = wmbus_apator_a,
    .len = WMBUS_APATOR_A_LEN,
    .is_t_raw = true,
    .expect_plausible = true,
    .expect_crc_ok = true,
};
static const WmBusTestVector wmbus_vector_t_apator_b_ok = {
    .name = "t_apator_b_ok",
    .data = wmbus_apator_b,
    .len = WMBUS_APATOR_B_LEN,
    .is_t_raw = true,
    .expect_plausible = true,
    .expect_crc_ok = true,
};
static const WmBusTestVector wmbus_vector_t_apator_c_ok = {
    .name = "t_apator_c_ok",
    .data = wmbus_apator_c,
    .len = WMBUS_APATOR_C_LEN,
    .is_t_raw = true,
    .expect_plausible = true,
    .expect_crc_ok = true,
};
static const WmBusTestVector wmbus_vector_t_apator_b_crc_bad = {
    .name = "t_apator_b_crc_bad",
    .data = wmbus_apator_b,
    .len = WMBUS_APATOR_B_LEN,
    .is_t_raw = true,
    .expect_plausible = true,
    .expect_crc_ok = false,
};
static const WmBusTestVector wmbus_vector_t_apator_b_bad_symbol = {
    .name = "t_apator_b_bad_symbol",
    .data = wmbus_apator_b,
    .len = WMBUS_APATOR_B_LEN,
    .is_t_raw = true,
    .expect_plausible = false,
    .expect_crc_ok = false,
};

static const WmBusSelftestCase wmbus_selftest_cases[] = {
    {.name = "c_apator_a_ok",
     .vector = &wmbus_vector_c_apator_a_ok,
     .build_format_a = true,
     .seed_corrupt_byte_pos = WMBUS_BIT_NONE,
     .frame_corrupt_byte_pos = WMBUS_BIT_NONE,
     .raw_corrupt_bit_pos = WMBUS_BIT_NONE},
    {.name = "c_apator_b_ok",
     .vector = &wmbus_vector_c_apator_b_ok,
     .build_format_a = true,
     .seed_corrupt_byte_pos = WMBUS_BIT_NONE,
     .frame_corrupt_byte_pos = WMBUS_BIT_NONE,
     .raw_corrupt_bit_pos = WMBUS_BIT_NONE},
    {.name = "c_apator_c_ok",
     .vector = &wmbus_vector_c_apator_c_ok,
     .build_format_a = true,
     .seed_corrupt_byte_pos = WMBUS_BIT_NONE,
     .frame_corrupt_byte_pos = WMBUS_BIT_NONE,
     .raw_corrupt_bit_pos = WMBUS_BIT_NONE},
    {.name = "c_apator_b_crc_bad",
     .vector = &wmbus_vector_c_apator_b_crc_bad,
     .build_format_a = true,
     .seed_corrupt_byte_pos = WMBUS_BIT_NONE,
     .frame_corrupt_byte_pos = WMBUS_BYTE_LAST,
     .raw_corrupt_bit_pos = WMBUS_BIT_NONE},
    {.name = "c_apator_b_bad_c_field",
     .vector = &wmbus_vector_c_apator_b_bad_c_field,
     .build_format_a = true,
     .seed_corrupt_byte_pos = 1U,
     .frame_corrupt_byte_pos = WMBUS_BIT_NONE,
     .raw_corrupt_bit_pos = WMBUS_BIT_NONE},
    {.name = "t_apator_a_ok",
     .vector = &wmbus_vector_t_apator_a_ok,
     .build_format_a = true,
     .seed_corrupt_byte_pos = WMBUS_BIT_NONE,
     .frame_corrupt_byte_pos = WMBUS_BIT_NONE,
     .raw_corrupt_bit_pos = WMBUS_BIT_NONE},
    {.name = "t_apator_b_ok",
     .vector = &wmbus_vector_t_apator_b_ok,
     .build_format_a = true,
     .seed_corrupt_byte_pos = WMBUS_BIT_NONE,
     .frame_corrupt_byte_pos = WMBUS_BIT_NONE,
     .raw_corrupt_bit_pos = WMBUS_BIT_NONE},
    {.name = "t_apator_c_ok",
     .vector = &wmbus_vector_t_apator_c_ok,
     .build_format_a = true,
     .seed_corrupt_byte_pos = WMBUS_BIT_NONE,
     .frame_corrupt_byte_pos = WMBUS_BIT_NONE,
     .raw_corrupt_bit_pos = WMBUS_BIT_NONE},
    {.name = "t_apator_b_crc_bad",
     .vector = &wmbus_vector_t_apator_b_crc_bad,
     .build_format_a = true,
     .seed_corrupt_byte_pos = WMBUS_BIT_NONE,
     .frame_corrupt_byte_pos = WMBUS_BYTE_LAST,
     .raw_corrupt_bit_pos = WMBUS_BIT_NONE},
    {.name = "t_apator_b_bad_symbol",
     .vector = &wmbus_vector_t_apator_b_bad_symbol,
     .build_format_a = true,
     .seed_corrupt_byte_pos = WMBUS_BIT_NONE,
     .frame_corrupt_byte_pos = WMBUS_BIT_NONE,
     .raw_corrupt_bit_pos = 0U},
};

static bool
    wmbus_selftest_run_c_case(const WmBusSelftestCase* test_case, WmBusSelftestResult* result) {
    const uint8_t* frame = NULL;
    size_t frame_len = 0;
    if(!wmbus_selftest_prepare_frame(test_case, &frame, &frame_len)) {
        wmbus_selftest_result_reset(result);
        return false;
    }

    if(!wmbus_selftest_run_phy_frame(
           WmBusRxModeC, WmBusFrameFormatA, frame, frame_len, NULL, result)) {
        return false;
    }

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

    if(!wmbus_selftest_generate_t_3of6_raw(
           frame,
           frame_len,
           raw,
           sizeof(raw),
           &raw_len,
           &raw_bit_len)) {
        wmbus_selftest_result_reset(result);
        return false;
    }

    wmbus_selftest_corrupt_t_raw_bit(raw, raw_bit_len, test_case->raw_corrupt_bit_pos);

    WmBusPhyFrame phy_frame = {0};
    if(!wmbus_phy_frame_from_fifo(WmBusRxModeT, raw, raw_len, -60, &phy_frame)) {
        wmbus_selftest_result_reset(result);
        return !test_case->vector->expect_plausible && !test_case->vector->expect_crc_ok;
    }
    if(!wmbus_selftest_run_phy_frame(
           phy_frame.mode,
           phy_frame.format,
           phy_frame.data,
           phy_frame.len,
           NULL,
           result)) {
        return false;
    }

    return (result->plausible == test_case->vector->expect_plausible) &&
           (result->crc_ok == test_case->vector->expect_crc_ok);
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
        if(pass) {
            FURI_LOG_I(
                TAG,
                "%s mode=T plausible=%s L=%s len=%s CRC=%s mfg=%s id=%s aligned=FIFO[0]",
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
                "%s mode=T plausible=%s L=%s len=%s CRC=%s mfg=%s id=%s aligned=FIFO[0]",
                test_case->name,
                result->plausible ? "YES" : "NO",
                l_field,
                computed_len,
                result->crc_ok ? "YES" : "NO",
                result->manufacturer,
                result->id);
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
        wmbus_selftest_write_report_line(
            file,
            "%s %s mode=T plausible=%s L=%s len=%s CRC=%s mfg=%s id=%s aligned=FIFO[0]\n",
            pass ? "PASS" : "FAIL",
            test_case->name,
            result->plausible ? "YES" : "NO",
            l_field,
            computed_len,
            result->crc_ok ? "YES" : "NO",
            result->manufacturer,
            result->id);
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
    WmBusPacketRecord record = {0};

    if(!wmbus_frame_build_format_a(
           wmbus_apator_b, WMBUS_APATOR_B_LEN, frame, sizeof(frame), &frame_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "build format-A failed");
        return false;
    }
    if(!wmbus_selftest_generate_t_3of6_raw(
           frame, frame_len, raw, sizeof(raw), &raw_len, &raw_bit_len)) {
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

    WmBusPhyFrame phy_frame = {0};
    if(!wmbus_phy_frame_from_fifo(
           WmBusRxModeT, capture, raw_len + tail_len, -60, &phy_frame) ||
       !wmbus_packet_process_phy_frame(&phy_frame, NULL, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "PHY conversion/process failed");
        return false;
    }
    if(!wmbus_packet_quality_meets(record.quality, WmBusPacketQualityCrcOk) ||
       strcmp(record.identity.meter_id, "21202020") != 0) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "unexpected quality=%u id=%s",
            (unsigned int)record.quality,
            record.identity.meter_id);
        return false;
    }

    wmbus_selftest_set_detail(
        detail, detail_len, "tail=%u decoded_by_packet_pipeline", (unsigned int)tail_len);
    return true;
}

static bool
    wmbus_selftest_check_packet_process_t_ignores_invalid_tail_1(char* detail, size_t detail_len) {
    return wmbus_selftest_check_packet_process_t_ignores_invalid_tail(detail, detail_len, 1U);
}

static bool
    wmbus_selftest_check_packet_process_t_ignores_invalid_tail_16(char* detail, size_t detail_len) {
    return wmbus_selftest_check_packet_process_t_ignores_invalid_tail(detail, detail_len, 16U);
}

static bool
    wmbus_selftest_check_packet_process_t_ignores_invalid_tail_64(char* detail, size_t detail_len) {
    return wmbus_selftest_check_packet_process_t_ignores_invalid_tail(detail, detail_len, 64U);
}

static bool
    wmbus_selftest_check_packet_process_t_rejects_fifo_prefix(char* detail, size_t detail_len) {
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t frame_len = 0;
    uint8_t raw[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t raw_len = 0;
    size_t raw_bit_len = 0;
    uint8_t capture[WMBUS_SELFTEST_BUF_MAX] = {0};

    if(!wmbus_frame_build_format_a(
           wmbus_apator_b, WMBUS_APATOR_B_LEN, frame, sizeof(frame), &frame_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "build format-A failed");
        return false;
    }
    if(!wmbus_selftest_generate_t_3of6_raw(
           frame, frame_len, raw, sizeof(raw), &raw_len, &raw_bit_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "generate raw failed");
        return false;
    }
    if(raw_len + 2U > sizeof(capture)) {
        wmbus_selftest_set_detail(
            detail, detail_len, "capture overflow raw_len=%u", (unsigned int)raw_len);
        return false;
    }

    capture[0] = 0x3CU;
    capture[1] = 0x94U;
    memcpy(&capture[2], raw, raw_len);

    WmBusPhyFrame phy_frame = {0};
    if(wmbus_phy_frame_from_fifo(WmBusRxModeT, capture, raw_len + 2U, -60, &phy_frame)) {
        wmbus_selftest_set_detail(detail, detail_len, "FIFO prefix unexpectedly decoded");
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "fifo_prefix=3C94 rejected");
    return true;
}

static bool wmbus_selftest_check_capture_c_accepts_access_demand(char* detail, size_t detail_len) {
    const uint8_t raw[] = {0x0A, 0x48, 0x01, 0x06, 0x20, 0x20, 0x20, 0x20, 0x05, 0x07, 0x7A};
    if(!wmbus_decode_is_plausible_frame(raw, sizeof(raw))) {
        wmbus_selftest_set_detail(detail, detail_len, "access demand rejected");
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "c_field=48 accepted");
    return true;
}

static bool wmbus_selftest_check_real_c_capture_24008355(
    const char* capture_hex,
    uint8_t expected_access,
    uint32_t expected_total_m3_x1000,
    char* detail,
    size_t detail_len) {
    uint8_t capture[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t capture_len = 0U;
    WmBusPhyFrame frame = {0};
    WmBusPacketRecord record = {0};
    uint32_t total_m3_x1000 = 0U;

    if(!wmbus_selftest_hex_to_bytes(capture_hex, capture, sizeof(capture), &capture_len) ||
       capture_len != sizeof(capture)) {
        wmbus_selftest_set_detail(detail, detail_len, "real capture decode failed len=%u", (unsigned int)capture_len);
        return false;
    }
    if(!wmbus_phy_frame_from_fifo(WmBusRxModeC, capture, capture_len, -60, &frame)) {
        wmbus_selftest_set_detail(detail, detail_len, "C FIFO sync validation failed");
        return false;
    }
    if(frame.len != 91U || frame.format != WmBusFrameFormatA) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "unexpected captured wire len=%u format=%u",
            (unsigned int)frame.len,
            (unsigned int)frame.format);
        return false;
    }
    if(!wmbus_packet_process_phy_frame(&frame, NULL, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "process failed");
        return false;
    }

    if(record.quality != WmBusPacketQualityParsed || record.packet_len != 79U ||
       strcmp(record.identity.manufacturer, "MAD") != 0 ||
       strcmp(record.identity.meter_id, "24008355") != 0 || record.dll.ci_field != 0x7AU ||
       record.tpl.acc != expected_access || record.application.parser_id != WmBusParserIdDifVif ||
       !wmbus_selftest_find_total_volume(&record, &total_m3_x1000) ||
       total_m3_x1000 != expected_total_m3_x1000) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "q=%u len=%u mfg=%s id=%s ci=%02X acc=%02X parser=%u total=%lu",
            (unsigned int)record.quality,
            (unsigned int)record.packet_len,
            record.identity.manufacturer,
            record.identity.meter_id,
            record.dll.ci_field,
            record.tpl.acc,
            (unsigned int)record.application.parser_id,
            (unsigned long)total_m3_x1000);
        return false;
    }

    wmbus_selftest_set_detail(
        detail,
        detail_len,
        "real C capture MAD/24008355 acc=%02X total=%lu",
        expected_access,
        (unsigned long)total_m3_x1000);
    return true;
}

static bool wmbus_selftest_check_real_c_capture_24008355_access_47(
    char* detail,
    size_t detail_len) {
    return wmbus_selftest_check_real_c_capture_24008355(
        wmbus_selftest_c_real_meter_capture_24008355_access_47,
        0x47U,
        29008U,
        detail,
        detail_len);
}

static bool wmbus_selftest_check_real_c_capture_24008355_access_48(
    char* detail,
    size_t detail_len) {
    return wmbus_selftest_check_real_c_capture_24008355(
        wmbus_selftest_c_real_meter_capture_24008355_access_48,
        0x48U,
        29009U,
        detail,
        detail_len);
}

static bool wmbus_selftest_check_c_capture_validates_sync_remainder(
    char* detail,
    size_t detail_len) {
    const uint8_t frame_a_fifo[] = {0x54U, 0xCDU, 0x4EU, 0x44U};
    const uint8_t frame_b_fifo[] = {0x54U, 0x3DU, 0x4EU, 0x44U};
    const uint8_t invalid_fifo[] = {0x54U, 0x00U, 0x4EU, 0x44U};
    size_t expected_fifo_len = 0U;
    WmBusFrameFormat format = WmBusFrameFormatUnknown;

    if(wmbus_fifo_frame_length(
           WmBusRxModeC,
           frame_a_fifo,
           sizeof(frame_a_fifo),
           &expected_fifo_len,
           &format) != WmBusCaptureLengthKnown ||
       expected_fifo_len != 93U || format != WmBusFrameFormatA) {
        wmbus_selftest_set_detail(detail, detail_len, "Frame-A 54CD remainder rejected");
        return false;
    }
    if(wmbus_fifo_frame_length(
           WmBusRxModeC,
           frame_b_fifo,
           sizeof(frame_b_fifo),
           &expected_fifo_len,
           &format) != WmBusCaptureLengthKnown ||
       expected_fifo_len != 81U || format != WmBusFrameFormatB) {
        wmbus_selftest_set_detail(detail, detail_len, "Frame-B 543D remainder rejected");
        return false;
    }
    if(wmbus_fifo_frame_length(
           WmBusRxModeC,
           invalid_fifo,
           sizeof(invalid_fifo),
           &expected_fifo_len,
           &format) != WmBusCaptureLengthInvalid) {
        wmbus_selftest_set_detail(detail, detail_len, "invalid 5400 remainder accepted");
        return false;
    }

    wmbus_selftest_set_detail(
        detail, detail_len, "hardware 543D + software Frame-A/Frame-B remainder validated");
    return true;
}

static bool wmbus_selftest_check_packet_process_c_bad_header_keeps_wire_diagnostic(
    char* detail,
    size_t detail_len) {
    const uint8_t raw[] = {0x24, 0x99, 0x00, 0x00, 0xA5, 0x5A};
    WmBusPacketRecord record = {0};

    if(!wmbus_selftest_process_phy_frame_record(
           WmBusRxModeC, WmBusFrameFormatA, raw, sizeof(raw), NULL, &record)) {
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

    wmbus_selftest_set_detail(detail, detail_len, "quality=Any capture wire diagnostic kept");
    return true;
}

static bool
    wmbus_selftest_check_frame_normalize_format_a_wire_frame(char* detail, size_t detail_len) {
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t frame_len = 0;
    uint8_t normalized[WMBUS_SELFTEST_BUF_MAX] = {0};
    WmBusFrameNormalizeResult result = {0};

    if(!wmbus_frame_build_format_a(
           wmbus_apator_a, WMBUS_APATOR_A_LEN, frame, sizeof(frame), &frame_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "build format-A failed");
        return false;
    }
    if(!wmbus_frame_normalize(
           WmBusFrameFormatA, frame, frame_len, normalized, sizeof(normalized), &result)) {
        wmbus_selftest_set_detail(detail, detail_len, "normalize format-A failed");
        return false;
    }
    if(!result.length_ok || !result.crc_known || !result.crc_ok ||
       result.format != WmBusFrameFormatA || result.normalized_len != WMBUS_APATOR_A_LEN ||
       memcmp(wmbus_apator_a, normalized, WMBUS_APATOR_A_LEN) != 0) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "unexpected format=%u len_ok=%u crc_ok=%u normalized_len=%u",
            (unsigned int)result.format,
            result.length_ok ? 1U : 0U,
            result.crc_ok ? 1U : 0U,
            (unsigned int)result.normalized_len);
        return false;
    }
    wmbus_selftest_set_detail(detail, detail_len, "format=A normalized_len=%u", 111U);
    return true;
}

static bool wmbus_selftest_check_frame_normalize_c_mode_format_a_wire_frame(
    char* detail,
    size_t detail_len) {
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t frame_len = 0;
    uint8_t normalized[WMBUS_SELFTEST_BUF_MAX] = {0};
    WmBusFrameNormalizeResult result = {0};

    if(!wmbus_frame_build_format_a(
           wmbus_apator_b, WMBUS_APATOR_B_LEN, frame, sizeof(frame), &frame_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "build format-A failed");
        return false;
    }
    if(!wmbus_frame_normalize(
           WmBusFrameFormatA, frame, frame_len, normalized, sizeof(normalized), &result)) {
        wmbus_selftest_set_detail(detail, detail_len, "normalize format-A failed");
        return false;
    }
    if(!result.length_ok || !result.crc_known || !result.crc_ok ||
       result.format != WmBusFrameFormatA || result.normalized_len != WMBUS_APATOR_B_LEN ||
       memcmp(wmbus_apator_b, normalized, WMBUS_APATOR_B_LEN) != 0) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "unexpected format=%u len_ok=%u crc_ok=%u normalized_len=%u",
            (unsigned int)result.format,
            result.length_ok ? 1U : 0U,
            result.crc_ok ? 1U : 0U,
            (unsigned int)result.normalized_len);
        return false;
    }
    wmbus_selftest_set_detail(detail, detail_len, "mode=C format=A normalized_len=%u", 79U);
    return true;
}

static bool
    wmbus_selftest_check_frame_normalize_format_b_wire_frame(char* detail, size_t detail_len) {
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t frame_len = 0;
    uint8_t normalized[WMBUS_SELFTEST_BUF_MAX] = {0};
    WmBusFrameNormalizeResult result = {0};

    if(!wmbus_frame_build_format_b(
           wmbus_apator_c, WMBUS_APATOR_C_LEN, frame, sizeof(frame), &frame_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "build format-B failed");
        return false;
    }
    if(!wmbus_frame_normalize(
           WmBusFrameFormatB, frame, frame_len, normalized, sizeof(normalized), &result)) {
        wmbus_selftest_set_detail(detail, detail_len, "normalize format-B failed");
        return false;
    }
    if(!result.length_ok || !result.crc_known || !result.crc_ok ||
       result.format != WmBusFrameFormatB || result.normalized_len != WMBUS_APATOR_C_LEN ||
       memcmp(wmbus_apator_c, normalized, WMBUS_APATOR_C_LEN) != 0) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "unexpected format=%u len_ok=%u crc_ok=%u normalized_len=%u",
            (unsigned int)result.format,
            result.length_ok ? 1U : 0U,
            result.crc_ok ? 1U : 0U,
            (unsigned int)result.normalized_len);
        return false;
    }
    wmbus_selftest_set_detail(detail, detail_len, "format=B normalized_len=%u", 63U);
    return true;
}

static bool wmbus_selftest_check_packet_process_c_crc_bad_keeps_complete_header(
    char* detail,
    size_t detail_len) {
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t frame_len = 0;
    uint8_t capture[WMBUS_SELFTEST_BUF_MAX] = {0};
    const size_t tail_len = 5U;
    WmBusPacketRecord record = {0};

    if(!wmbus_frame_build_format_b(
           wmbus_apator_c, WMBUS_APATOR_C_LEN, frame, sizeof(frame), &frame_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "build format-B failed");
        return false;
    }
    if(frame_len + tail_len > sizeof(capture)) {
        wmbus_selftest_set_detail(detail, detail_len, "capture overflow");
        return false;
    }

    frame[5] ^= 0x01U;

    if(wmbus_frame_crc_check(WmBusFrameFormatB, frame, frame_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "corrupt frame unexpectedly passed CRC");
        return false;
    }

    WmBusFrameMeasureResult measure = {0};
    if(!wmbus_frame_measure(WmBusFrameFormatB, frame, frame_len, &measure) || !measure.complete ||
       measure.format != WmBusFrameFormatB || measure.frame_len != frame_len ||
       measure.normalized_len != WMBUS_APATOR_C_LEN) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "unexpected measure complete=%u format=%u wire=%u norm=%u",
            measure.complete ? 1U : 0U,
            (unsigned int)measure.format,
            (unsigned int)measure.frame_len,
            (unsigned int)measure.normalized_len);
        return false;
    }

    uint8_t normalized[WMBUS_SELFTEST_BUF_MAX] = {0};
    WmBusFrameNormalizeResult normalize = {0};
    if(wmbus_frame_normalize(
           WmBusFrameFormatB, frame, frame_len, normalized, sizeof(normalized), &normalize) ||
       normalize.length_ok || normalize.crc_known || normalize.crc_ok) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "unexpected normalize len_ok=%u crc_known=%u crc_ok=%u",
            normalize.length_ok ? 1U : 0U,
            normalize.crc_known ? 1U : 0U,
            normalize.crc_ok ? 1U : 0U);
        return false;
    }

    memcpy(capture, frame, frame_len);
    memset(&capture[frame_len], 0xA5, tail_len);

    if(!wmbus_selftest_process_phy_frame_record(
           WmBusRxModeC,
           WmBusFrameFormatB,
           capture,
           frame_len + tail_len,
           NULL,
           &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "process failed");
        return false;
    }

    if(record.quality != WmBusPacketQualityFrameComplete || record.packet_len != frame_len ||
       memcmp(record.packet_bytes, frame, frame_len) != 0 ||
       record.application.parser_id != WmBusParserIdShortTpl) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "unexpected quality=%u packet_len=%u parser=%u",
            (unsigned int)record.quality,
            (unsigned int)record.packet_len,
            (unsigned int)record.application.parser_id);
        return false;
    }

    wmbus_selftest_set_detail(
        detail, detail_len, "mode=C crc_bad complete header kept tail=%u", (unsigned int)tail_len);
    return true;
}

static bool wmbus_selftest_check_c_format_a_is_authoritative(char* detail, size_t detail_len) {
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t frame_len = 0U;
    WmBusPacketRecord record = {0};

    if(!wmbus_frame_build_format_a(
           wmbus_apator_b, WMBUS_APATOR_B_LEN, frame, sizeof(frame), &frame_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "build format-A failed");
        return false;
    }
    frame[5] ^= 0x01U;

    if(!wmbus_selftest_process_phy_frame_record(
           WmBusRxModeC,
           WmBusFrameFormatA,
           frame,
           frame_len,
           NULL,
           &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "process failed");
        return false;
    }

    if(record.quality != WmBusPacketQualityFrameComplete ||
       record.format != WmBusFrameFormatA || record.packet_len != frame_len ||
       record.packet_len == wmbus_frame_len_format_b(frame[0])) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "format-A lost quality=%u wire=%u stored=%u format-B=%u",
            (unsigned int)record.quality,
            (unsigned int)frame_len,
            (unsigned int)record.packet_len,
            (unsigned int)wmbus_frame_len_format_b(frame[0]));
        return false;
    }

    wmbus_selftest_set_detail(
        detail, detail_len, "format=A authoritative wire_len=%u", (unsigned int)frame_len);
    return true;
}

static bool wmbus_selftest_check_fifo_safe_read_sizes(char* detail, size_t detail_len) {
    const size_t t_first = wmbus_fifo_safe_read_size(
        WmBusRxModeT, WmBusCaptureLengthNeedMore, 0U, 0U, 2U);
    const size_t c_first = wmbus_fifo_safe_read_size(
        WmBusRxModeC, WmBusCaptureLengthNeedMore, 0U, 0U, 3U);
    const size_t stream = wmbus_fifo_safe_read_size(
        WmBusRxModeC, WmBusCaptureLengthKnown, 3U, 93U, 64U);
    const size_t prefetch_full = wmbus_fifo_safe_read_size(
        WmBusRxModeC, WmBusCaptureLengthKnown, 3U, 93U, 65U);
    const size_t retain = wmbus_fifo_safe_read_size(
        WmBusRxModeC, WmBusCaptureLengthKnown, 66U, 93U, 26U);
    const size_t final = wmbus_fifo_safe_read_size(
        WmBusRxModeC, WmBusCaptureLengthKnown, 66U, 93U, 27U);

    if(t_first != 1U || c_first != 2U || stream != 63U || prefetch_full != 64U ||
       retain != 25U || final != 27U) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "unexpected reads T=%u C=%u stream=%u prefetch=%u retain=%u final=%u",
            (unsigned int)t_first,
            (unsigned int)c_first,
            (unsigned int)stream,
            (unsigned int)prefetch_full,
            (unsigned int)retain,
            (unsigned int)final);
        return false;
    }

    wmbus_selftest_set_detail(
        detail, detail_len, "65-byte prefetch state drains 64-byte FIFO with guard");
    return true;
}

static bool wmbus_selftest_check_capture_state_reset(char* detail, size_t detail_len) {
    WmBusFifoCaptureState state = {0};

    wmbus_fifo_capture_note_activity(&state, 1234U);
    if(!state.in_packet || state.last_activity_tick != 1234U) {
        wmbus_selftest_set_detail(detail, detail_len, "activity ownership failed");
        return false;
    }
    state.raw_len = 9U;

    wmbus_fifo_capture_state_reset(&state);

    if(state.raw_len != 0U || state.in_packet || state.last_activity_tick != 0U) {
        wmbus_selftest_set_detail(detail, detail_len, "state reset failed");
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "activity_owned=YES state_reset=YES");
    return true;
}

static const WmBusSelftestCheck wmbus_selftest_checks_modes[] = {
    {"check_packet_process_t_ignores_invalid_tail_1",
     wmbus_selftest_check_packet_process_t_ignores_invalid_tail_1},
    {"check_packet_process_t_ignores_invalid_tail_16",
     wmbus_selftest_check_packet_process_t_ignores_invalid_tail_16},
    {"check_packet_process_t_ignores_invalid_tail_64",
     wmbus_selftest_check_packet_process_t_ignores_invalid_tail_64},
    {"check_packet_process_t_rejects_fifo_prefix",
     wmbus_selftest_check_packet_process_t_rejects_fifo_prefix},
    {"check_capture_c_accepts_access_demand",
     wmbus_selftest_check_capture_c_accepts_access_demand},
    {"check_real_c_capture_24008355_access_47",
     wmbus_selftest_check_real_c_capture_24008355_access_47},
    {"check_real_c_capture_24008355_access_48",
     wmbus_selftest_check_real_c_capture_24008355_access_48},
    {"check_c_capture_validates_sync_remainder",
     wmbus_selftest_check_c_capture_validates_sync_remainder},
    {"check_packet_process_c_bad_header_keeps_wire_diagnostic",
     wmbus_selftest_check_packet_process_c_bad_header_keeps_wire_diagnostic},
    {"check_frame_normalize_format_a_wire_frame",
     wmbus_selftest_check_frame_normalize_format_a_wire_frame},
    {"check_frame_normalize_c_mode_format_a_wire_frame",
     wmbus_selftest_check_frame_normalize_c_mode_format_a_wire_frame},
    {"check_frame_normalize_format_b_wire_frame",
     wmbus_selftest_check_frame_normalize_format_b_wire_frame},
    {"check_packet_process_c_crc_bad_keeps_complete_header",
     wmbus_selftest_check_packet_process_c_crc_bad_keeps_complete_header},
    {"check_c_format_a_is_authoritative", wmbus_selftest_check_c_format_a_is_authoritative},
    {"check_fifo_safe_read_sizes", wmbus_selftest_check_fifo_safe_read_sizes},
    {"check_capture_state_reset", wmbus_selftest_check_capture_state_reset},
};

const WmBusSelftestCheck* wmbus_selftest_mode_checks(size_t* count) {
    if(count) *count = COUNT_OF(wmbus_selftest_checks_modes);
    return wmbus_selftest_checks_modes;
}
