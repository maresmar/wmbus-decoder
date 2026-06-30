#include "wmbus_selftest_i.h"

#include "../protocol/decode/wmbus_decode.h"
#include "../protocol/format/wmbus_packet_formatter.h"
#include "../protocol/format/wmbus_record_formatter.h"
#include "../protocol/model/wmbus_application_record.h"
#include "../protocol/parser/wmbus_parser.h"
#include "../protocol/parser/wmbus_parser_apator162.h"
#include "../protocol/parser/wmbus_parser_dif_vif.h"

#include <string.h>

static bool wmbus_selftest_check_3of6_valid_single_byte(char* detail, size_t detail_len) {
    const uint8_t raw[] = {0x58, 0xD0};
    uint8_t out[4] = {0};
    size_t out_len = 0;

    if(!wmbus_decode_3of6_bits(raw, sizeof(raw) * 8U, 0U, out, sizeof(out), &out_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "decode failed");
        return false;
    }
    if(out_len != 1U || out[0] != 0x01U) {
        wmbus_selftest_set_detail(detail, detail_len, "unexpected out_len=%u out0=%02X", (unsigned int)out_len, out[0]);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "decoded=01 len=1");
    return true;
}

static bool wmbus_selftest_check_3of6_valid_single_byte_offset_1(char* detail, size_t detail_len) {
    const uint8_t raw[] = {0x2C, 0x68, 0x00};
    uint8_t out[4] = {0};
    size_t out_len = 0;

    if(!wmbus_decode_3of6_bits(raw, 17U, 1U, out, sizeof(out), &out_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "decode failed at offset=1");
        return false;
    }
    if(out_len != 1U || out[0] != 0x01U) {
        wmbus_selftest_set_detail(detail, detail_len, "unexpected out_len=%u out0=%02X", (unsigned int)out_len, out[0]);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "decoded=01 len=1 offset=1");
    return true;
}

static bool wmbus_selftest_check_3of6_reject_dangling_nibble(char* detail, size_t detail_len) {
    const uint8_t raw[] = {0x58};
    uint8_t out[4] = {0};
    size_t out_len = 0;

    if(wmbus_decode_3of6_bits(raw, sizeof(raw) * 8U, 0U, out, sizeof(out), &out_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "dangling nibble accepted");
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "reject=YES");
    return true;
}

static bool wmbus_selftest_check_3of6_reject_invalid_symbol(char* detail, size_t detail_len) {
    const uint8_t raw[] = {0x00};
    uint8_t out[4] = {0};
    size_t out_len = 0;

    if(wmbus_decode_3of6_bits(raw, sizeof(raw) * 8U, 0U, out, sizeof(out), &out_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "invalid symbol accepted");
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "reject=YES");
    return true;
}

static bool wmbus_selftest_check_parser_plausibility(char* detail, size_t detail_len) {
    const uint8_t valid[] = {10, 0x44, 0x01, 0x06, 0, 0, 0, 0, 0, 0, 0};
    const uint8_t bad_c[] = {10, 0x45, 0x01, 0x06, 0, 0, 0, 0, 0, 0, 0};
    const uint8_t bad_mfg[] = {10, 0x44, 0x00, 0x00, 0, 0, 0, 0, 0, 0, 0};

    if(!wmbus_decode_is_plausible_frame(valid, sizeof(valid))) {
        wmbus_selftest_set_detail(detail, detail_len, "valid frame rejected");
        return false;
    }
    if(wmbus_decode_is_plausible_frame(bad_c, sizeof(bad_c))) {
        wmbus_selftest_set_detail(detail, detail_len, "bad C-field accepted");
        return false;
    }
    if(wmbus_decode_is_plausible_frame(bad_mfg, sizeof(bad_mfg))) {
        wmbus_selftest_set_detail(detail, detail_len, "bad manufacturer accepted");
        return false;
    }
    if(wmbus_decode_is_plausible_frame(valid, 10U)) {
        wmbus_selftest_set_detail(detail, detail_len, "too-short frame accepted");
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "valid=YES bad_c=NO bad_mfg=NO short=NO");
    return true;
}

static bool wmbus_selftest_check_apator162_register_sizes(char* detail, size_t detail_len) {
    int size_10 = wmbus_parser_apator162_register_size(0x10);
    int size_a1 = wmbus_parser_apator162_register_size(0xA1);
    int size_b2 = wmbus_parser_apator162_register_size(0xB2);
    int size_fe = wmbus_parser_apator162_register_size(0xFE);

    if(size_10 != 4 || size_a1 != 4 || size_b2 != 16 || size_fe != -1) {
        wmbus_selftest_set_detail(detail, detail_len, "sizes 10=%d A1=%d B2=%d FE=%d", size_10, size_a1, size_b2, size_fe);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "10=4 A1=4 B2=16 FE=-1");
    return true;
}

static bool wmbus_selftest_check_short_tpl_security_modes(char* detail, size_t detail_len) {
    uint8_t clear_mode = wmbus_parser_short_tpl_security_mode(0x0000U);
    uint8_t aes_cbc_iv_mode = wmbus_parser_short_tpl_security_mode(0x8560U);
    uint8_t mfct_mode = wmbus_parser_short_tpl_security_mode(0x0100U);
    uint8_t aes_ctr_cmac_mode = wmbus_parser_short_tpl_security_mode(0x0800U);

    if(clear_mode != 0U || aes_cbc_iv_mode != 5U || mfct_mode != 1U || aes_ctr_cmac_mode != 8U) {
        wmbus_selftest_set_detail(detail, detail_len, "unexpected modes clear=%u aes=%u mfct=%u ctr=%u", (unsigned int)clear_mode, (unsigned int)aes_cbc_iv_mode, (unsigned int)mfct_mode, (unsigned int)aes_ctr_cmac_mode);
        return false;
    }

    if(wmbus_parser_short_tpl_security_likely_encrypted(0x0000U) ||
       !wmbus_parser_short_tpl_security_likely_encrypted(0x8560U) ||
       wmbus_parser_short_tpl_security_likely_encrypted(0x0100U) ||
       !wmbus_parser_short_tpl_security_likely_encrypted(0x0800U)) {
        wmbus_selftest_set_detail(detail, detail_len, "security encryption heuristic mismatch");
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "clear=%u aes=%u ctr=%u", (unsigned int)clear_mode, (unsigned int)aes_cbc_iv_mode, (unsigned int)aes_ctr_cmac_mode);
    return true;
}

static bool wmbus_selftest_check_ell_security_modes(char* detail, size_t detail_len) {
    uint32_t clear_sn = 0x01234567UL;
    uint32_t aes_ctr_sn = 0x21234567UL;

    if(!wmbus_parser_ci_has_ell(0x8DU) || !wmbus_parser_ci_has_ell(0x8EU) ||
       !wmbus_parser_ci_has_ell(0x8FU) || wmbus_parser_ci_has_ell(0x7AU)) {
        wmbus_selftest_set_detail(detail, detail_len, "ci detection mismatch");
        return false;
    }

    if(!wmbus_parser_ell_has_session_fields(0x8DU) ||
       wmbus_parser_ell_has_session_fields(0x8EU) ||
       !wmbus_parser_ell_has_session_fields(0x8FU)) {
        wmbus_selftest_set_detail(detail, detail_len, "session-field detection mismatch");
        return false;
    }

    if(wmbus_parser_ell_security_mode(clear_sn) != 0U ||
       wmbus_parser_ell_security_mode(aes_ctr_sn) != 1U ||
       wmbus_parser_ell_security_likely_encrypted(clear_sn) ||
       !wmbus_parser_ell_security_likely_encrypted(aes_ctr_sn)) {
        wmbus_selftest_set_detail(detail, detail_len, "security heuristic mismatch clear=%u aes=%u", (unsigned int)wmbus_parser_ell_security_mode(clear_sn), (unsigned int)wmbus_parser_ell_security_mode(aes_ctr_sn));
        return false;
    }

    wmbus_selftest_set_detail(
        detail, detail_len, "8D/8F session AES-CTR mode=%u", (unsigned int)wmbus_parser_ell_security_mode(aes_ctr_sn));
    return true;
}

static bool wmbus_selftest_check_dif_vif_decode_basic(char* detail, size_t detail_len) {
    static const uint8_t payload[] = {
        0x04, 0x13, 0x40, 0xE2, 0x01, 0x00, 0x0C, 0x03, 0x56, 0x34, 0x12, 0x00,
        0x84, 0x01, 0x80, 0x13, 0x78, 0x56, 0x34, 0x12, 0x04, 0x38, 0x4E, 0x61,
        0xBC, 0x00, 0x02, 0x5A, 0xD7, 0x00, 0x02, 0x5E, 0xA0, 0x00, 0x02, 0x62,
        0x37, 0x00, 0x02, 0x6C, 0x2C, 0x33, 0x04, 0x6D, 0x25, 0x10, 0x2C, 0x33,
        0x02, 0xFD, 0x17, 0x34, 0x12,
    };
    WmBusApplicationRecord records[WMBUS_PACKET_RECORD_MAX] = {0};
    uint8_t count = 0U;
    FuriString* fields = furi_string_alloc();
    if(!fields) {
        wmbus_selftest_set_detail(detail, detail_len, "alloc failed");
        return false;
    }

    if(!wmbus_packet_decode_application_records(payload, sizeof(payload), records, COUNT_OF(records), &count)) {
        wmbus_selftest_set_detail(detail, detail_len, "decode failed");
        furi_string_free(fields);
        return false;
    }

    wmbus_record_formatter_format_joined(records, count, ';', fields);
    const char* fields_text = furi_string_get_cstr(fields);

    if(count != 10U || records[0].quantity != WmBusApplicationQuantityVolume ||
       records[0].value_type != WmBusApplicationValueUnsigned || records[0].value_unsigned != 123456U ||
       !strstr(fields_text, "Volume[inst]=123.456 m3") ||
       records[1].quantity != WmBusApplicationQuantityEnergy ||
       records[1].value_type != WmBusApplicationValueUnsigned || records[1].value_unsigned != 123456U ||
       !strstr(fields_text, "Energy[inst]=123456 Wh") || records[2].storage_no != 2U ||
       records[3].quantity != WmBusApplicationQuantityVolumeFlow ||
       !strstr(fields_text, "Flow=12.345678 m3/h") ||
       records[4].quantity != WmBusApplicationQuantityFlowTemperature || records[4].value_type != WmBusApplicationValueUnsigned ||
       records[4].value_unsigned != 215U || records[4].scale10 != -1 ||
       records[5].quantity != WmBusApplicationQuantityReturnTemperature || records[5].value_type != WmBusApplicationValueUnsigned ||
       records[5].value_unsigned != 160U || records[5].scale10 != -1 ||
       records[6].quantity != WmBusApplicationQuantityTemperatureDifference || records[6].value_type != WmBusApplicationValueUnsigned ||
       records[6].value_unsigned != 55U || records[6].scale10 != -1 ||
       records[7].quantity != WmBusApplicationQuantityDate || records[7].value_type != WmBusApplicationValueDateTime ||
       records[7].value_datetime.year != 2025U || records[7].value_datetime.month != 3U ||
       records[7].value_datetime.day != 12U || records[7].value_datetime.has_time ||
       !strstr(fields_text, "Date=2025-03-12") ||
       records[8].quantity != WmBusApplicationQuantityDateTime ||
       records[8].value_type != WmBusApplicationValueDateTime || records[8].value_datetime.year != 2025U ||
       records[8].value_datetime.month != 3U || records[8].value_datetime.day != 12U ||
       !records[8].value_datetime.has_time || records[8].value_datetime.hour != 16U ||
       records[8].value_datetime.minute != 37U ||
       !strstr(fields_text, "Measured at=2025-03-12 16:37") ||
       records[9].quantity != WmBusApplicationQuantityStatus || records[9].value_type != WmBusApplicationValueRaw ||
       !strstr(fields_text, "Status=3412")) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "count=%u fields=%s",
            (unsigned int)count,
            fields_text);
        furi_string_free(fields);
        return false;
    }

    furi_string_free(fields);
    wmbus_selftest_set_detail(detail, detail_len, "records=%u common_types=volume,energy,flow,temp,date,status", (unsigned int)count);
    return true;
}

static bool wmbus_selftest_check_dif_vif_decode_reject_malformed(char* detail, size_t detail_len) {
    static const uint8_t malformed[] = {0x04, 0x13, 0x34, 0x12};
    WmBusApplicationRecord records[WMBUS_PACKET_RECORD_MAX] = {0};
    uint8_t count = 0U;

    if(wmbus_packet_decode_application_records(malformed, sizeof(malformed), records, COUNT_OF(records), &count)) {
        wmbus_selftest_set_detail(detail, detail_len, "malformed payload accepted count=%u", (unsigned int)count);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "malformed payload rejected=YES");
    return true;
}

static bool wmbus_selftest_check_format_fields_text_prefers_primary_records(
    char* detail,
    size_t detail_len) {
    WmBusPacketApplicationData application = {0};
    WmBusApplicationRecord* record = NULL;
    FuriString* fields = furi_string_alloc();

    if(!fields) {
        wmbus_selftest_set_detail(detail, detail_len, "fields alloc failed");
        return false;
    }

    if(!wmbus_application_record_append(&application, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "append volume primary failed");
        furi_string_free(fields);
        return false;
    }
    record->quantity = WmBusApplicationQuantityVolume;
    record->measurement_type = WmBusApplicationMeasurementTypeInstantaneous;
    record->scale10 = -3;
    record->data_len = 4U;
    wmbus_application_record_set_unsigned(record, 150U);

    if(!wmbus_application_record_append(&application, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "append volume history failed");
        furi_string_free(fields);
        return false;
    }
    record->quantity = WmBusApplicationQuantityVolume;
    record->measurement_type = WmBusApplicationMeasurementTypeInstantaneous;
    record->storage_no = 1U;
    record->scale10 = -5;
    record->data_len = 6U;
    wmbus_application_record_set_unsigned(record, 9395877606541289160ULL);

    if(!wmbus_application_record_append(&application, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "append energy primary failed");
        furi_string_free(fields);
        return false;
    }
    record->quantity = WmBusApplicationQuantityEnergy;
    record->measurement_type = WmBusApplicationMeasurementTypeInstantaneous;
    record->scale10 = -2;
    record->data_len = 2U;
    wmbus_application_record_set_unsigned(record, 2U);

    if(!wmbus_application_record_append(&application, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "append energy tariff failed");
        furi_string_free(fields);
        return false;
    }
    record->quantity = WmBusApplicationQuantityEnergy;
    record->measurement_type = WmBusApplicationMeasurementTypeInstantaneous;
    record->tariff = 1U;
    record->scale10 = 0;
    record->data_len = 6U;
    wmbus_application_record_set_unsigned(record, 468274118951ULL);

    if(!wmbus_application_record_append(&application, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "append status failed");
        furi_string_free(fields);
        return false;
    }
    record->quantity = WmBusApplicationQuantityStatus;
    record->measurement_type = WmBusApplicationMeasurementTypeInstantaneous;
    if(!wmbus_application_record_set_raw_hex_le(record, (const uint8_t[]){0x92, 0x01}, 2U)) {
        wmbus_selftest_set_detail(detail, detail_len, "status raw encode failed");
        furi_string_free(fields);
        return false;
    }
    record->data_len = 2U;

    wmbus_record_formatter_format_joined(
        application.records, application.record_count, ';', fields);

    if(strcmp(furi_string_get_cstr(fields), "Volume[inst]=0.150 m3;Energy[inst]=0.02 Wh;Status=9201") != 0) {
        wmbus_selftest_set_detail(detail, detail_len, "fields=%s", furi_string_get_cstr(fields));
        furi_string_free(fields);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "fields=%s", furi_string_get_cstr(fields));
    furi_string_free(fields);
    return true;
}

static bool wmbus_selftest_check_packet_quality_policy(char* detail, size_t detail_len) {
    WmBusPacketRecord record = {
        .has_capture = true,
        .plausible = true,
        .length_ok = true,
        .crc_known = true,
        .crc_ok = true,
        .rssi = -82,
    };
    record.quality = wmbus_packet_quality_from_record(&record);

    if(record.quality != WmBusPacketQualityCrcOk) {
        wmbus_selftest_set_detail(
            detail, detail_len, "unexpected quality=%u", (unsigned int)record.quality);
        return false;
    }
    if(!wmbus_packet_record_passes_policy(&record, WmBusPacketQualityCrcOk, 0)) {
        wmbus_selftest_set_detail(detail, detail_len, "disabled RSSI gate rejected record");
        return false;
    }
    if(!wmbus_packet_record_passes_policy(&record, WmBusPacketQualityHeaderOk, -85)) {
        wmbus_selftest_set_detail(detail, detail_len, "enabled RSSI gate rejected strong-enough record");
        return false;
    }
    if(wmbus_packet_record_passes_policy(&record, WmBusPacketQualityHeaderOk, -80)) {
        wmbus_selftest_set_detail(detail, detail_len, "enabled RSSI gate accepted weak record");
        return false;
    }
    if(wmbus_packet_record_passes_policy(&record, WmBusPacketQualityParsed, 0)) {
        wmbus_selftest_set_detail(detail, detail_len, "quality gate accepted unparsed record");
        return false;
    }

    record.parsed_ok = true;
    record.quality = wmbus_packet_quality_from_record(&record);
    if(record.quality != WmBusPacketQualityParsed ||
       !wmbus_packet_record_passes_policy(&record, WmBusPacketQualityParsed, 0)) {
        wmbus_selftest_set_detail(
            detail, detail_len, "parsed quality failed quality=%u", (unsigned int)record.quality);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "policy rssi_off=YES rssi_gate=YES quality=YES");
    return true;
}

static const WmBusSelftestCheck wmbus_selftest_checks_tooling[] = {
    {"check_3of6_valid_single_byte", wmbus_selftest_check_3of6_valid_single_byte},
    {"check_3of6_valid_single_byte_offset_1", wmbus_selftest_check_3of6_valid_single_byte_offset_1},
    {"check_3of6_reject_dangling_nibble", wmbus_selftest_check_3of6_reject_dangling_nibble},
    {"check_3of6_reject_invalid_symbol", wmbus_selftest_check_3of6_reject_invalid_symbol},
    {"check_parser_plausibility", wmbus_selftest_check_parser_plausibility},
    {"check_apator162_register_sizes", wmbus_selftest_check_apator162_register_sizes},
    {"check_short_tpl_security_modes", wmbus_selftest_check_short_tpl_security_modes},
    {"check_ell_security_modes", wmbus_selftest_check_ell_security_modes},
    {"check_dif_vif_decode_basic", wmbus_selftest_check_dif_vif_decode_basic},
    {"check_dif_vif_decode_reject_malformed", wmbus_selftest_check_dif_vif_decode_reject_malformed},
    {"check_format_fields_text_prefers_primary_records",
     wmbus_selftest_check_format_fields_text_prefers_primary_records},
    {"check_packet_quality_policy", wmbus_selftest_check_packet_quality_policy},
};

const WmBusSelftestCheck* wmbus_selftest_tooling_checks(size_t* count) {
    if(count) *count = COUNT_OF(wmbus_selftest_checks_tooling);
    return wmbus_selftest_checks_tooling;
}
