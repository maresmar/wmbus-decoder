#include "wmbus_packet_security.h"

#include <furi.h>
#include <string.h>

#include "../crypto/wmbus_crypto_mode5.h"
#include "../frame/wmbus_frame.h"
#include "../parser/wmbus_device_parser.h"
#include "../parser/wmbus_parser.h"

#define TAG "WmBusDecoder"

static const char* wmbus_packet_decrypt_result_str(WmBusDecryptResult result) {
    switch(result) {
    case WmBusDecryptResultOk:
        return "ok";
    case WmBusDecryptResultInvalidArgs:
        return "invalid args";
    case WmBusDecryptResultFrameTooShort:
        return "frame too short";
    case WmBusDecryptResultEncryptedPayloadTooShort:
        return "encrypted payload shorter than one AES block";
    default:
        return "unknown";
    }
}

static const char* wmbus_packet_log_id(const WmBusPacketRecord* record) {
    if(!record) return "--------";
    return record->identity.meter_id[0] ? record->identity.meter_id : "--------";
}

static void wmbus_packet_log_decrypt_failure_reason(
    const WmBusPacketRecord* record,
    const uint8_t key[WMBUS_CRYPTO_KEY_BYTES],
    const char* reason) {
    if(!record || !key || !reason || reason[0] == '\0') return;

    FURI_LOG_D(
        TAG,
        "decrypt failed id=%s ci=%02X cfg=%04X key=%02X%02X.. reason=%s",
        wmbus_packet_log_id(record),
        record->dll.ci_field,
        record->tpl.cfg,
        key[0],
        key[1],
        reason);
}

static void wmbus_packet_log_decrypt_failure(
    const WmBusPacketRecord* record,
    const uint8_t key[WMBUS_CRYPTO_KEY_BYTES],
    WmBusDecryptResult result) {
    if(!record || !key || result == WmBusDecryptResultOk) return;

    wmbus_packet_log_decrypt_failure_reason(record, key, wmbus_packet_decrypt_result_str(result));
}

static void wmbus_packet_reset_application_payload(WmBusPacketPayloadData* payload) {
    if(!payload) return;

    payload->has_application_payload = false;
    payload->application_len = 0U;
}

static void wmbus_packet_copy_application_payload(
    WmBusPacketPayloadData* payload,
    const uint8_t* frame,
    size_t frame_len,
    size_t offset) {
    if(!payload || !frame) return;

    if(frame_len <= offset) {
        wmbus_packet_reset_application_payload(payload);
        return;
    }

    size_t payload_len = frame_len - offset;
    if(payload_len > sizeof(payload->application_bytes)) {
        payload_len = sizeof(payload->application_bytes);
    }

    payload->has_application_payload = true;
    payload->application_len = (uint16_t)payload_len;
    memcpy(payload->application_bytes, &frame[offset], payload_len);
}

static void wmbus_packet_set_payload_packet_slice(WmBusPacketRecord* record, size_t frame_len) {
    if(!record) return;

    size_t offset = record->tpl.header_len;
    record->payload.packet_offset = 0U;
    record->payload.packet_len = 0U;
    if(frame_len <= offset) return;

    size_t payload_len = frame_len - offset;
    if(payload_len > (record->packet_len - offset)) {
        payload_len = record->packet_len - offset;
    }
    if(payload_len > UINT16_MAX) payload_len = UINT16_MAX;

    record->payload.packet_offset = (uint16_t)offset;
    record->payload.packet_len = (uint16_t)payload_len;
}

static bool wmbus_packet_try_key(
    const uint8_t* frame,
    size_t frame_len,
    const uint8_t key[WMBUS_CRYPTO_KEY_BYTES],
    WmBusPacketRecord* record,
    WmBusPacketPayloadData* out_payload,
    uint8_t decrypt_frame[WMBUS_PACKET_APPLICATION_MAX]) {
    if(!frame || !key || !record || !out_payload || !decrypt_frame) {
        return false;
    }

    WmBusMode5DecryptInfo decrypt =
        wmbus_crypto_mode5_decrypt(frame, frame_len, record->tpl.cfg, key, decrypt_frame);
    if(decrypt.result != WmBusDecryptResultOk) {
        wmbus_packet_log_decrypt_failure(record, key, decrypt.result);
        return false;
    }

    WmBusParserPacketView parser_packet = {
        .dll = &record->dll,
        .tpl = &record->tpl,
        .payload = &record->payload,
        .identity = &record->identity,
    };

    WmBusPacketPayloadData candidate_payload = record->payload;
    wmbus_packet_copy_application_payload(
        &candidate_payload, decrypt_frame, frame_len, record->tpl.header_len);
    parser_packet.payload = &candidate_payload;

    if(decrypt.has_check_bytes || wmbus_device_parser_validate_decrypt(&parser_packet, NULL)) {
        *out_payload = candidate_payload;
        return true;
    }

    wmbus_packet_log_decrypt_failure_reason(record, key, "missing 2F2F check bytes");
    return false;
}

bool wmbus_packet_resolve_application_payload(
    const uint8_t* frame,
    size_t frame_len,
    WmBusPacketRecord* record,
    const WmBusCryptoKeyStore* key_store) {
    if(!frame || !record) {
        return false;
    }

    record->tpl.decrypted = false;
    record->tpl.key_index = 0U;

    wmbus_packet_set_payload_packet_slice(record, frame_len);
    wmbus_packet_reset_application_payload(&record->payload);

    bool has_check_bytes = record->payload.packet_len >= 2U &&
                           record->packet_bytes[record->payload.packet_offset] == 0x2FU &&
                           record->packet_bytes[record->payload.packet_offset + 1U] == 0x2FU;
    bool needs_decrypt = record->tpl.has_short_tpl &&
                         wmbus_parser_short_tpl_security_likely_encrypted(record->tpl.cfg);
    if(has_check_bytes || !needs_decrypt) {
        wmbus_packet_copy_application_payload(
            &record->payload, frame, frame_len, record->tpl.header_len);
        return true;
    }

    if(record->tpl.security_mode != 0x05U) {
        FURI_LOG_D(
            TAG,
            "decrypt skipped id=%s ci=%02X cfg=%04X unsupported security mode=%02X",
            wmbus_packet_log_id(record),
            record->dll.ci_field,
            record->tpl.cfg,
            record->tpl.security_mode);
        return false;
    }

    uint8_t decrypt_frame[WMBUS_PACKET_APPLICATION_MAX] = {0};

    for(uint8_t i = 0; key_store && i < key_store->count; i++) {
        const uint8_t* key = wmbus_crypto_key_store_get(key_store, i);
        if(!key) {
            continue;
        }

        if(wmbus_packet_try_key(frame, frame_len, key, record, &record->payload, decrypt_frame)) {
            record->tpl.decrypted = true;
            record->tpl.key_index = i;
            return true;
        }
    }

    return false;
}
