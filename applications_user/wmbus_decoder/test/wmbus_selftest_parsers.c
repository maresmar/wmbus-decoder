#include "wmbus_selftest_i.h"

#include "../protocol/decode/wmbus_decode.h"
#include "../protocol/parser/wmbus_parser.h"

#include <string.h>

static const char* wmbus_selftest_parser_name(const WmBusPacketRecord* record) {
    return record ? wmbus_parser_id_name(record->application.parser_id) : "Unknown";
}

static bool wmbus_selftest_check_parser_apator162_public_vectors(char* detail, size_t detail_len) {
    for(size_t i = 0; i < WMBUS_SELFTEST_APATOR_PUBLIC_VECTOR_COUNT; i++) {
        const WmBusSelftestApatorPublicVector* vector = &wmbus_selftest_apator_public_vectors[i];
        uint8_t normalized[WMBUS_SELFTEST_BUF_MAX] = {0};
        size_t normalized_len = 0;
        uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
        size_t frame_len = 0;
        uint8_t roundtrip[WMBUS_SELFTEST_BUF_MAX] = {0};
        WmBusFrameNormalizeResult result = {0};
        WmBusPacketRecord record = {0};
        char id[WMBUS_ID_STR_LEN] = {0};
        char rec_desc[96] = {0};

        if(!wmbus_selftest_hex_to_bytes(vector->telegram, normalized, sizeof(normalized), &normalized_len)) {
            wmbus_selftest_set_detail(detail, detail_len, "vector %s hex parse failed", vector->id);
            return false;
        }
        if(normalized_len != vector->parsed_len) {
            wmbus_selftest_set_detail(detail, detail_len, "vector %s unexpected len=%u", vector->id, (unsigned int)normalized_len);
            return false;
        }
        if(wmbus_selftest_fnv1a32(normalized, normalized_len) != vector->parsed_fnv1a) {
            wmbus_selftest_set_detail(detail, detail_len, "vector %s fingerprint mismatch", vector->id);
            return false;
        }
        if(normalized[0] != (uint8_t)(normalized_len - 1U) ||
           !wmbus_decode_is_plausible_frame(normalized, normalized_len)) {
            wmbus_selftest_set_detail(detail, detail_len, "vector %s plausibility failed", vector->id);
            return false;
        }
        if(!wmbus_frame_build_format_a(normalized, normalized_len, frame, sizeof(frame), &frame_len)) {
            wmbus_selftest_set_detail(detail, detail_len, "vector %s format-A build failed", vector->id);
            return false;
        }
        if(!wmbus_frame_normalize(WmBusRxModeT, frame, frame_len, roundtrip, sizeof(roundtrip), &result)) {
            wmbus_selftest_set_detail(detail, detail_len, "vector %s normalize failed", vector->id);
            return false;
        }
        if(!result.length_ok || !result.crc_known || !result.crc_ok || result.format != WmBusFrameFormatA ||
           result.normalized_len != normalized_len || memcmp(normalized, roundtrip, normalized_len) != 0) {
            wmbus_selftest_set_detail(detail, detail_len, "vector %s roundtrip failed", vector->id);
            return false;
        }
        wmbus_frame_format_id(&roundtrip[4], id, NULL);
        if(strncmp(vector->id, id, WMBUS_ID_STR_LEN) != 0) {
            wmbus_selftest_set_detail(detail, detail_len, "vector expected id=%s got=%s", vector->id, id);
            return false;
        }
        if(!wmbus_selftest_process_capture_record(WmBusRxModeC, roundtrip, result.normalized_len, NULL, &record)) {
            wmbus_selftest_set_detail(detail, detail_len, "vector %s packet process failed", vector->id);
            return false;
        }

        uint32_t total_m3_x1000 = 0U;
        wmbus_selftest_describe_first_record(&record, rec_desc, sizeof(rec_desc));
        if(record.application.parser_id != WmBusParserIdApator162 ||
           !wmbus_selftest_find_total_volume(&record, &total_m3_x1000) ||
           total_m3_x1000 != vector->total_m3_x1000 ||
           strcmp(record.identity.meter_id, vector->id) != 0 ||
           record.tpl.decrypted || record.tpl.key_index != 0U) {
            wmbus_selftest_set_detail(detail, detail_len, "vector %s parser=%s total=%lu expected=%lu id=%s enc=%s dec=%s idx=%u %s", vector->id, wmbus_selftest_parser_name(&record), (unsigned long)total_m3_x1000, (unsigned long)vector->total_m3_x1000, record.identity.meter_id, (record.tpl.has_short_tpl && wmbus_parser_short_tpl_security_likely_encrypted(record.tpl.cfg)) ? "YES" : "NO", record.tpl.decrypted ? "YES" : "NO", (unsigned int)record.tpl.key_index, rec_desc);
            return false;
        }
    }

    wmbus_selftest_set_detail(detail, detail_len, "vectors=%u", (unsigned int)WMBUS_SELFTEST_APATOR_PUBLIC_VECTOR_COUNT);
    return true;
}

static bool wmbus_selftest_check_parser_apator162_old_style_ci_b6_rejected(char* detail, size_t detail_len) {
    uint8_t normalized[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t normalized_len = 0;
    WmBusPacketRecord record = {0};

    if(!wmbus_selftest_hex_to_bytes(wmbus_selftest_apator_old_style_b6, normalized, sizeof(normalized), &normalized_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "hex parse failed");
        return false;
    }
    if(normalized_len != wmbus_selftest_apator_old_style_b6_len) {
        wmbus_selftest_set_detail(detail, detail_len, "unexpected len=%u", (unsigned int)normalized_len);
        return false;
    }
    if(wmbus_selftest_fnv1a32(normalized, normalized_len) != wmbus_selftest_apator_old_style_b6_fnv1a) {
        wmbus_selftest_set_detail(detail, detail_len, "fingerprint mismatch");
        return false;
    }
    if(normalized_len <= 10U || normalized[10] != 0xB6U) {
        wmbus_selftest_set_detail(detail, detail_len, "expected CI=B6");
        return false;
    }
    if(!wmbus_selftest_process_capture_record(WmBusRxModeC, normalized, normalized_len, NULL, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "packet process failed");
        return false;
    }
    uint32_t total_m3_x1000 = 0U;
    if(record.application.parser_id == WmBusParserIdApator162 ||
       wmbus_selftest_find_total_volume(&record, &total_m3_x1000)) {
        wmbus_selftest_set_detail(detail, detail_len, "old-style CI=B6 accepted parser=%s total=%lu", wmbus_selftest_parser_name(&record), (unsigned long)total_m3_x1000);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "ci=B6 rejected=YES");
    return true;
}

static bool
    wmbus_selftest_check_parser_apator162_mode5_configured_zero_key_vectors(
        char* detail,
        size_t detail_len) {
    const WmBusSelftestApatorFieldVector vectors[] = {
        {wmbus_selftest_apator_encrypted_mode5, 4848U, "88888888"},
        {wmbus_selftest_apator_encrypted_mode5_gold, 345654U, "02991056"},
        {wmbus_selftest_apator_encrypted_mode5_field_02991035, 200257U, "02991035"},
    };
    WmBusCryptoKeyStore key_store = {0};

    key_store.count = 1U;

    for(size_t i = 0; i < COUNT_OF(vectors); i++) {
        uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
        size_t frame_len = 0;
        WmBusPacketRecord record = {0};
        uint16_t cfg = 0;
        char rec_desc[96] = {0};

        if(!wmbus_selftest_hex_to_format_b_frame(vectors[i].telegram, frame, sizeof(frame), &frame_len)) {
            wmbus_selftest_set_detail(detail, detail_len, "vector %s format-B build failed", vectors[i].id);
            return false;
        }
        if(frame_len < 17U || frame[10] != 0x7AU) {
            wmbus_selftest_set_detail(detail, detail_len, "vector %s unexpected frame", vectors[i].id);
            return false;
        }
        cfg = (uint16_t)frame[13] | ((uint16_t)frame[14] << 8);
        if(!wmbus_parser_short_tpl_security_likely_encrypted(cfg) || (frame[15] == 0x2FU && frame[16] == 0x2FU)) {
            wmbus_selftest_set_detail(detail, detail_len, "vector %s unexpected cipher state", vectors[i].id);
            return false;
        }
        if(!wmbus_selftest_process_capture_record(WmBusRxModeC, frame, frame_len, &key_store, &record)) {
            wmbus_selftest_set_detail(detail, detail_len, "vector %s packet process failed", vectors[i].id);
            return false;
        }

        uint32_t total_m3_x1000 = 0U;
        wmbus_selftest_describe_first_record(&record, rec_desc, sizeof(rec_desc));
        if(record.application.parser_id != WmBusParserIdApator162 ||
           !wmbus_selftest_find_total_volume(&record, &total_m3_x1000) ||
           total_m3_x1000 != vectors[i].total_m3_x1000 || !record.tpl.decrypted ||
           record.tpl.key_index != 0U ||
           strcmp(record.identity.meter_id, vectors[i].id) != 0) {
            wmbus_selftest_set_detail(detail, detail_len, "vector %s parser=%s total=%lu dec=%s idx=%u id=%s %s", vectors[i].id, wmbus_selftest_parser_name(&record), (unsigned long)total_m3_x1000, record.tpl.decrypted ? "YES" : "NO", (unsigned int)record.tpl.key_index, record.identity.meter_id, rec_desc);
            return false;
        }
    }

    wmbus_selftest_set_detail(detail, detail_len, "mode5 configured-zero vectors=%u key_index=0", (unsigned int)COUNT_OF(vectors));
    return true;
}

static bool wmbus_selftest_check_packet_process_mode5_no_key_does_not_decrypt(
    char* detail,
    size_t detail_len) {
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t frame_len = 0;
    WmBusCryptoKeyStore key_store = {0};
    WmBusPacketRecord record = {0};

    if(!wmbus_selftest_hex_to_format_b_frame(wmbus_selftest_apator_encrypted_mode5, frame, sizeof(frame), &frame_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "format-B build failed");
        return false;
    }
    if(!wmbus_selftest_process_capture_record(WmBusRxModeC, frame, frame_len, &key_store, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "packet process failed");
        return false;
    }

    if(record.application.parser_id != WmBusParserIdShortTpl || record.tpl.decrypted ||
       record.tpl.key_index != 0U || record.application.record_count != 0U ||
       record.payload.has_application_payload) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "unexpected parser=%s dec=%s idx=%u records=%u payload=%s",
            wmbus_selftest_parser_name(&record),
            record.tpl.decrypted ? "YES" : "NO",
            (unsigned int)record.tpl.key_index,
            (unsigned int)record.application.record_count,
            record.payload.has_application_payload ? "YES" : "NO");
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "no_key leaves encrypted payload closed");
    return true;
}

static bool wmbus_selftest_check_packet_process_mode5_parser_configured_zero_key(
    char* detail,
    size_t detail_len) {
    const WmBusSelftestApatorFieldVector vectors[] = {
        {wmbus_selftest_apator_encrypted_mode5_gold, 345654U, "02991056"},
        {wmbus_selftest_apator_encrypted_mode5_field_02991035, 200257U, "02991035"},
    };
    WmBusCryptoKeyStore key_store = {0};

    key_store.count = 1U;

    for(size_t i = 0; i < COUNT_OF(vectors); i++) {
        uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
        size_t frame_len = 0;
        WmBusPacketRecord record = {0};
        char rec_desc[96] = {0};

        if(!wmbus_selftest_hex_to_format_b_frame(vectors[i].telegram, frame, sizeof(frame), &frame_len)) {
            wmbus_selftest_set_detail(detail, detail_len, "vector %s format-B build failed", vectors[i].id);
            return false;
        }
        if(!wmbus_selftest_process_capture_record(WmBusRxModeC, frame, frame_len, &key_store, &record)) {
            wmbus_selftest_set_detail(detail, detail_len, "vector %s packet process failed", vectors[i].id);
            return false;
        }

        uint32_t total_m3_x1000 = 0U;
        wmbus_selftest_describe_first_record(&record, rec_desc, sizeof(rec_desc));
        if(record.application.parser_id != WmBusParserIdApator162 ||
           !wmbus_selftest_find_total_volume(&record, &total_m3_x1000) ||
           total_m3_x1000 != vectors[i].total_m3_x1000 || !record.tpl.decrypted ||
           record.tpl.key_index != 0U ||
           strcmp(record.identity.meter_id, vectors[i].id) != 0) {
            wmbus_selftest_set_detail(detail, detail_len, "vector %s parser=%s total=%lu dec=%s idx=%u id=%s %s", vectors[i].id, wmbus_selftest_parser_name(&record), (unsigned long)total_m3_x1000, record.tpl.decrypted ? "YES" : "NO", (unsigned int)record.tpl.key_index, record.identity.meter_id, rec_desc);
            return false;
        }
    }

    wmbus_selftest_set_detail(detail, detail_len, "configured_zero_key vectors=%u key_index=0", (unsigned int)COUNT_OF(vectors));
    return true;
}

static bool wmbus_selftest_check_packet_process_mode5_configured_zero_key_slot(char* detail, size_t detail_len) {
    const WmBusSelftestApatorFieldVector vectors[] = {
        {wmbus_selftest_apator_encrypted_mode5_gold, 345654U, "02991056"},
        {wmbus_selftest_apator_encrypted_mode5_field_02991035, 200257U, "02991035"},
    };
    WmBusCryptoKeyStore key_store = {0};

    key_store.count = 1U;
    for(size_t i = 0; i < COUNT_OF(vectors); i++) {
        uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
        size_t frame_len = 0;
        WmBusPacketRecord record = {0};
        char rec_desc[96] = {0};

        if(!wmbus_selftest_hex_to_format_b_frame(vectors[i].telegram, frame, sizeof(frame), &frame_len)) {
            wmbus_selftest_set_detail(detail, detail_len, "vector %s format-B build failed", vectors[i].id);
            return false;
        }
        if(!wmbus_selftest_process_capture_record(WmBusRxModeC, frame, frame_len, &key_store, &record)) {
            wmbus_selftest_set_detail(detail, detail_len, "vector %s packet process failed", vectors[i].id);
            return false;
        }

        uint32_t total_m3_x1000 = 0U;
        wmbus_selftest_describe_first_record(&record, rec_desc, sizeof(rec_desc));
        if(record.application.parser_id != WmBusParserIdApator162 ||
           !wmbus_selftest_find_total_volume(&record, &total_m3_x1000) ||
           total_m3_x1000 != vectors[i].total_m3_x1000 || !record.tpl.decrypted ||
           record.tpl.key_index != 0U ||
           strcmp(record.identity.meter_id, vectors[i].id) != 0) {
            wmbus_selftest_set_detail(detail, detail_len, "vector %s parser=%s total=%lu dec=%s idx=%u id=%s %s", vectors[i].id, wmbus_selftest_parser_name(&record), (unsigned long)total_m3_x1000, record.tpl.decrypted ? "YES" : "NO", (unsigned int)record.tpl.key_index, record.identity.meter_id, rec_desc);
            return false;
        }
    }

    wmbus_selftest_set_detail(detail, detail_len, "configured_zero_key vectors=%u key_index=0", (unsigned int)COUNT_OF(vectors));
    return true;
}

static bool wmbus_selftest_check_packet_process_mode5_multiple_keys_uses_matching_slot(
    char* detail,
    size_t detail_len) {
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t frame_len = 0;
    WmBusCryptoKeyStore key_store = {0};
    WmBusPacketRecord record = {0};
    uint32_t total_m3_x1000 = 0U;
    char rec_desc[96] = {0};

    memset(key_store.keys[0], 0xA5, sizeof(key_store.keys[0]));
    key_store.count = 2U;

    if(!wmbus_selftest_hex_to_format_b_frame(
           wmbus_selftest_apator_encrypted_mode5_gold, frame, sizeof(frame), &frame_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "format-B build failed");
        return false;
    }
    if(!wmbus_selftest_process_capture_record(WmBusRxModeC, frame, frame_len, &key_store, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "packet process failed");
        return false;
    }

    wmbus_selftest_describe_first_record(&record, rec_desc, sizeof(rec_desc));
    if(record.application.parser_id != WmBusParserIdApator162 ||
       !wmbus_selftest_find_total_volume(&record, &total_m3_x1000) ||
       total_m3_x1000 != 345654U || !record.tpl.decrypted || record.tpl.key_index != 1U ||
       strcmp(record.identity.meter_id, "02991056") != 0) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "parser=%s total=%lu dec=%s idx=%u id=%s %s",
            wmbus_selftest_parser_name(&record),
            (unsigned long)total_m3_x1000,
            record.tpl.decrypted ? "YES" : "NO",
            (unsigned int)record.tpl.key_index,
            record.identity.meter_id,
            rec_desc);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "multiple_keys matched key_index=1");
    return true;
}

static bool wmbus_selftest_check_packet_process_mode5_wrong_key_does_not_decrypt(
    char* detail,
    size_t detail_len) {
    const WmBusSelftestApatorFieldVector vectors[] = {
        {wmbus_selftest_apator_encrypted_mode5_gold, 345654U, "02991056"},
        {wmbus_selftest_apator_encrypted_mode5_field_02991035, 200257U, "02991035"},
    };
    WmBusCryptoKeyStore key_store = {0};

    memset(key_store.keys[0], 0xA5, sizeof(key_store.keys[0]));
    key_store.count = 1U;

    for(size_t i = 0; i < COUNT_OF(vectors); i++) {
        uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
        size_t frame_len = 0;
        WmBusPacketRecord record = {0};

        if(!wmbus_selftest_hex_to_format_b_frame(vectors[i].telegram, frame, sizeof(frame), &frame_len)) {
            wmbus_selftest_set_detail(detail, detail_len, "vector %s format-B build failed", vectors[i].id);
            return false;
        }
        if(!wmbus_selftest_process_capture_record(WmBusRxModeC, frame, frame_len, &key_store, &record)) {
            wmbus_selftest_set_detail(detail, detail_len, "vector %s packet process failed", vectors[i].id);
            return false;
        }

        if(record.application.parser_id != WmBusParserIdShortTpl || record.tpl.decrypted ||
           record.tpl.key_index != 0U || record.application.record_count != 0U ||
           record.payload.has_application_payload) {
            wmbus_selftest_set_detail(
                detail,
                detail_len,
                "vector %s parser=%s dec=%s idx=%u records=%u payload=%s",
                vectors[i].id,
                wmbus_selftest_parser_name(&record),
                record.tpl.decrypted ? "YES" : "NO",
                (unsigned int)record.tpl.key_index,
                (unsigned int)record.application.record_count,
                record.payload.has_application_payload ? "YES" : "NO");
            return false;
        }
    }

    wmbus_selftest_set_detail(detail, detail_len, "wrong_key=MISS no_fallback vectors=%u", (unsigned int)COUNT_OF(vectors));
    return true;
}

static bool wmbus_selftest_check_parser_apator162_mode5_corrupt_rejected(char* detail, size_t detail_len) {
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t frame_len = 0;
    WmBusPacketRecord record = {0};

    if(!wmbus_selftest_hex_to_format_b_frame(wmbus_selftest_apator_encrypted_mode5_corrupt, frame, sizeof(frame), &frame_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "format-B build failed");
        return false;
    }
    if(!wmbus_selftest_process_capture_record(WmBusRxModeC, frame, frame_len, NULL, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "packet process failed");
        return false;
    }

    uint32_t total_m3_x1000 = 0U;
    if(record.application.parser_id == WmBusParserIdApator162 ||
       wmbus_selftest_find_total_volume(&record, &total_m3_x1000) || record.tpl.decrypted) {
        wmbus_selftest_set_detail(detail, detail_len, "corrupt ciphertext accepted parser=%s total=%lu dec=%s", wmbus_selftest_parser_name(&record), (unsigned long)total_m3_x1000, record.tpl.decrypted ? "YES" : "NO");
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "mode5 corrupt rejected=YES");
    return true;
}

static bool wmbus_selftest_check_parser_apator162_payload_without_total(char* detail, size_t detail_len) {
    uint8_t frame[WMBUS_APATOR_B_LEN] = {0};
    WmBusPacketRecord record = {0};
    uint32_t total_m3_x1000 = 0U;

    memcpy(frame, wmbus_apator_b, sizeof(frame));
    frame[28] = 0xFFU;

    if(!wmbus_selftest_process_capture_record(WmBusRxModeC, frame, sizeof(frame), NULL, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "packet process failed");
        return false;
    }

    if(record.application.parser_id == WmBusParserIdApator162 ||
       wmbus_selftest_find_total_volume(&record, &total_m3_x1000)) {
        wmbus_selftest_set_detail(detail, detail_len, "payload unexpectedly parsed total=%lu", (unsigned long)total_m3_x1000);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "payload total=NO");
    return true;
}

static bool wmbus_selftest_check_parser_apator162_payload_total_a1(char* detail, size_t detail_len) {
    uint8_t frame[WMBUS_APATOR_B_LEN] = {0};
    WmBusPacketRecord record = {0};
    uint32_t total_m3_x1000 = 0U;

    memcpy(frame, wmbus_apator_b, sizeof(frame));
    frame[28] = 0xA1U;
    frame[29] = 0x78U;
    frame[30] = 0x56U;
    frame[31] = 0x34U;
    frame[32] = 0x12U;

    if(!wmbus_selftest_process_capture_record(WmBusRxModeC, frame, sizeof(frame), NULL, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "packet process failed");
        return false;
    }

    if(record.application.parser_id != WmBusParserIdApator162 ||
       !wmbus_selftest_find_total_volume(&record, &total_m3_x1000) ||
       total_m3_x1000 != 0x12345678U) {
        wmbus_selftest_set_detail(detail, detail_len, "a1 total parse failed total=%lu parser=%s", (unsigned long)total_m3_x1000, wmbus_selftest_parser_name(&record));
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "payload a1 total=%lu", (unsigned long)total_m3_x1000);
    return true;
}

static bool wmbus_selftest_check_parser_apator162_invalid_payload_not_claimed(char* detail, size_t detail_len) {
    uint8_t frame[WMBUS_APATOR_B_LEN] = {0};
    WmBusPacketRecord record = {0};

    memcpy(frame, wmbus_apator_b, sizeof(frame));
    frame[25] = 0xFEU;

    if(!wmbus_selftest_process_capture_record(WmBusRxModeC, frame, sizeof(frame), NULL, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "packet process failed");
        return false;
    }

    uint32_t total_m3_x1000 = 0U;
    if(record.application.parser_id == WmBusParserIdApator162 ||
       wmbus_selftest_find_total_volume(&record, &total_m3_x1000)) {
        wmbus_selftest_set_detail(detail, detail_len, "invalid payload accepted parser=%s total=%lu", wmbus_selftest_parser_name(&record), (unsigned long)total_m3_x1000);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "invalid payload rejected=YES");
    return true;
}

static bool wmbus_selftest_check_packet_sections_clear_payload(char* detail, size_t detail_len) {
    WmBusPacketRecord record = {0};

    if(!wmbus_selftest_process_capture_record(WmBusRxModeC, wmbus_apator_b, WMBUS_APATOR_B_LEN, NULL, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "packet process failed");
        return false;
    }
    if(record.payload.packet_len == 0U ||
       (uint32_t)record.payload.packet_offset + (uint32_t)record.payload.packet_len >
           record.packet_len ||
       record.packet_bytes[record.payload.packet_offset] != 0x2FU ||
       record.packet_bytes[record.payload.packet_offset + 1U] != 0x2FU) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "clear payload slice invalid raw=%u off=%u pkt=%u",
            (unsigned int)record.payload.packet_len,
            (unsigned int)record.payload.packet_offset,
            (unsigned int)record.packet_len);
        return false;
    }

    wmbus_selftest_set_detail(
        detail,
        detail_len,
        "clear_payload raw=%u off=%u",
        (unsigned int)record.payload.packet_len,
        (unsigned int)record.payload.packet_offset);
    return true;
}

static bool wmbus_selftest_check_packet_sections_encrypted_payload(char* detail, size_t detail_len) {
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t frame_len = 0U;
    WmBusCryptoKeyStore key_store = {0};
    WmBusPacketRecord record = {0};

    key_store.count = 1U;

    if(!wmbus_selftest_hex_to_format_b_frame(wmbus_selftest_apator_encrypted_mode5, frame, sizeof(frame), &frame_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "format-B build failed");
        return false;
    }
    if(!wmbus_selftest_process_capture_record(WmBusRxModeC, frame, frame_len, &key_store, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "packet process failed");
        return false;
    }
    if(!record.tpl.decrypted || record.tpl.key_index != 0U || record.payload.packet_len == 0U ||
       (uint32_t)record.payload.packet_offset + (uint32_t)record.payload.packet_len >
           record.packet_len ||
       (record.packet_len >= record.payload.packet_offset + 2U &&
        record.packet_bytes[record.payload.packet_offset] == 0x2FU &&
        record.packet_bytes[record.payload.packet_offset + 1U] == 0x2FU)) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "encrypted payload slice invalid dec=%s raw=%u off=%u",
            record.tpl.decrypted ? "YES" : "NO",
            (unsigned int)record.payload.packet_len,
            (unsigned int)record.payload.packet_offset);
        return false;
    }

    wmbus_selftest_set_detail(
        detail,
        detail_len,
        "encrypted raw=%u off=%u dec=YES",
        (unsigned int)record.payload.packet_len,
        (unsigned int)record.payload.packet_offset);
    return true;
}

static bool wmbus_selftest_check_packet_sections_unsupported_decrypt(char* detail, size_t detail_len) {
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t frame_len = 0U;
    WmBusPacketRecord record = {0};

    if(!wmbus_selftest_hex_to_format_b_frame(wmbus_selftest_apator_encrypted_mode5, frame, sizeof(frame), &frame_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "format-B build failed");
        return false;
    }
    if(frame_len < 15U) {
        wmbus_selftest_set_detail(detail, detail_len, "unexpected normalized len");
        return false;
    }
    frame[14] = 0x88U;

    if(!wmbus_selftest_process_capture_record(WmBusRxModeC, frame, frame_len, NULL, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "packet process failed");
        return false;
    }
    if(record.application.record_count != 0U || record.tpl.decrypted) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "unsupported decrypt records=%u dec=%s",
            (unsigned int)record.application.record_count,
            record.tpl.decrypted ? "YES" : "NO");
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "unsupported decrypt keeps packet slice only");
    return true;
}

static const WmBusSelftestCheck wmbus_selftest_checks_parsers[] = {
    {"check_parser_apator162_public_vectors", wmbus_selftest_check_parser_apator162_public_vectors},
    {"check_parser_apator162_old_style_ci_b6_rejected", wmbus_selftest_check_parser_apator162_old_style_ci_b6_rejected},
    {"check_parser_apator162_mode5_configured_zero_key_vectors", wmbus_selftest_check_parser_apator162_mode5_configured_zero_key_vectors},
    {"check_parser_apator162_payload_without_total", wmbus_selftest_check_parser_apator162_payload_without_total},
    {"check_parser_apator162_payload_total_a1", wmbus_selftest_check_parser_apator162_payload_total_a1},
    {"check_parser_apator162_invalid_payload_not_claimed", wmbus_selftest_check_parser_apator162_invalid_payload_not_claimed},
    {"check_packet_process_mode5_no_key_does_not_decrypt", wmbus_selftest_check_packet_process_mode5_no_key_does_not_decrypt},
    {"check_packet_process_mode5_parser_configured_zero_key", wmbus_selftest_check_packet_process_mode5_parser_configured_zero_key},
    {"check_packet_process_mode5_configured_zero_key_slot", wmbus_selftest_check_packet_process_mode5_configured_zero_key_slot},
    {"check_packet_process_mode5_multiple_keys_uses_matching_slot", wmbus_selftest_check_packet_process_mode5_multiple_keys_uses_matching_slot},
    {"check_packet_process_mode5_wrong_key_does_not_decrypt", wmbus_selftest_check_packet_process_mode5_wrong_key_does_not_decrypt},
    {"check_parser_apator162_mode5_corrupt_rejected", wmbus_selftest_check_parser_apator162_mode5_corrupt_rejected},
    {"check_packet_sections_clear_payload", wmbus_selftest_check_packet_sections_clear_payload},
    {"check_packet_sections_encrypted_payload", wmbus_selftest_check_packet_sections_encrypted_payload},
    {"check_packet_sections_unsupported_decrypt", wmbus_selftest_check_packet_sections_unsupported_decrypt},
};

const WmBusSelftestCheck* wmbus_selftest_parser_checks(size_t* count) {
    if(count) *count = COUNT_OF(wmbus_selftest_checks_parsers);
    return wmbus_selftest_checks_parsers;
}
