#include "wmbus_packet_security.h"

#include <furi.h>
#include <string.h>

#include "../frame/wmbus_frame.h"
#include "../parser/wmbus_device_parser.h"
#include "../parser/wmbus_parser.h"

#define TAG                 "WmBusDecoder"
#define WMBUS_DECODE_MAX    256U
#define WMBUS_MODE5_KEY_LEN 16U

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
    const uint8_t key[WMBUS_MODE5_KEY_LEN],
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
    const uint8_t key[WMBUS_MODE5_KEY_LEN],
    WmBusDecryptResult result) {
    if(!record || !key || result == WmBusDecryptResultOk) return;

    wmbus_packet_log_decrypt_failure_reason(record, key, wmbus_packet_decrypt_result_str(result));
}

static void wmbus_packet_parse_context_reset(WmBusPacketParseContext* parse_context) {
    if(!parse_context) return;

    parse_context->has_application_payload = false;
    parse_context->application_len = 0U;
    parse_context->application_payload = NULL;
}

static void wmbus_packet_parse_context_set_payload_view(
    WmBusPacketParseContext* parse_context,
    const uint8_t* frame,
    size_t frame_len,
    size_t offset) {
    if(!parse_context || !frame) return;

    if(frame_len <= offset) {
        wmbus_packet_parse_context_reset(parse_context);
        return;
    }

    size_t payload_len = frame_len - offset;
    if(payload_len > WMBUS_PACKET_PAYLOAD_MAX) {
        payload_len = WMBUS_PACKET_PAYLOAD_MAX;
    }

    parse_context->has_application_payload = true;
    parse_context->application_len = (uint16_t)payload_len;
    parse_context->application_payload = &frame[offset];
}

static void wmbus_packet_parse_context_copy_payload(
    WmBusPacketParseContext* parse_context,
    const uint8_t* frame,
    size_t frame_len,
    size_t offset) {
    if(!parse_context || !frame) return;

    if(frame_len <= offset) {
        wmbus_packet_parse_context_reset(parse_context);
        return;
    }

    size_t payload_len = frame_len - offset;
    if(payload_len > sizeof(parse_context->application_payload_storage)) {
        payload_len = sizeof(parse_context->application_payload_storage);
    }

    parse_context->has_application_payload = true;
    parse_context->application_len = (uint16_t)payload_len;
    memcpy(parse_context->application_payload_storage, &frame[offset], payload_len);
    parse_context->application_payload = parse_context->application_payload_storage;
}

static void wmbus_packet_set_payload_packet_slice(
    WmBusPacketRecord* record,
    size_t frame_len) {
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

static bool wmbus_packet_parser_validates_decrypt(const WmBusPacketApplicationData* application) {
    if(!application) {
        return false;
    }

    switch(application->parser_id) {
    case WmBusParserIdApator162:
        return true;
    default:
        return false;
    }
}

static bool wmbus_packet_try_key(
    const uint8_t* frame,
    size_t frame_len,
    const uint8_t key[WMBUS_MODE5_KEY_LEN],
    WmBusPacketRecord* record,
    WmBusPacketParseContext* parse_context,
    uint8_t decrypt_frame[WMBUS_DECODE_MAX]) {
    if(!frame || !key || !record || !parse_context || !decrypt_frame) {
        return false;
    }

    WmBusMode5DecryptInfo decrypt =
        wmbus_parser_decrypt_mode5(frame, frame_len, record->tpl.cfg, key, decrypt_frame);
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

    wmbus_packet_parse_context_copy_payload(
        parse_context, decrypt_frame, frame_len, record->tpl.header_len);
    if((wmbus_device_parser_apply(&parser_packet, parse_context, &record->application) &&
        wmbus_packet_parser_validates_decrypt(&record->application)) ||
       decrypt.has_check_bytes) {
        return true;
    }

    wmbus_packet_log_decrypt_failure_reason(record, key, "missing 2F2F check bytes");
    return false;
}

bool wmbus_packet_select_application(
    const uint8_t* frame,
    size_t frame_len,
    WmBusPacketRecord* record,
    WmBusPacketParseContext* parse_context,
    const WmBusKeyring* keyring) {
    static const uint8_t wmbus_zero_key[WMBUS_MODE5_KEY_LEN] = {0};

    if(!frame || !record || !parse_context) {
        return false;
    }

    record->tpl.decrypted = false;
    record->tpl.key_index = 0U;
    memset(&record->application, 0, sizeof(record->application));

    WmBusParserPacketView parser_packet = {
        .dll = &record->dll,
        .tpl = &record->tpl,
        .payload = &record->payload,
        .identity = &record->identity,
    };

    wmbus_packet_set_payload_packet_slice(record, frame_len);
    wmbus_packet_parse_context_reset(parse_context);

    if(record->payload.packet_len >= 2U &&
       record->packet_bytes[record->payload.packet_offset] == 0x2FU &&
       record->packet_bytes[record->payload.packet_offset + 1U] == 0x2FU) {
        wmbus_packet_parse_context_set_payload_view(
            parse_context, frame, frame_len, record->tpl.header_len);
        return wmbus_device_parser_apply(&parser_packet, parse_context, &record->application);
    }

    if(!record->tpl.has_short_tpl ||
       !wmbus_parser_short_tpl_security_likely_encrypted(record->tpl.cfg)) {
        wmbus_packet_parse_context_set_payload_view(
            parse_context, frame, frame_len, record->tpl.header_len);
        return wmbus_device_parser_apply(&parser_packet, parse_context, &record->application);
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

    uint8_t decrypt_frame[WMBUS_DECODE_MAX] = {0};

    for(uint8_t i = 0; keyring && i < keyring->count; i++) {
        const WmBusKeyEntry* entry = wmbus_keyring_get(keyring, i);
        if(!entry) continue;

        if(wmbus_packet_try_key(
               frame, frame_len, entry->key, record, parse_context, decrypt_frame)) {
            record->tpl.decrypted = true;
            record->tpl.key_index = i + 1U;
            return true;
        }
    }

    if(wmbus_packet_try_key(
           frame, frame_len, wmbus_zero_key, record, parse_context, decrypt_frame)) {
        record->tpl.decrypted = true;
        record->tpl.key_index = 0U;
        return true;
    }

    memset(&record->application, 0, sizeof(record->application));
    wmbus_packet_parse_context_reset(parse_context);
    return wmbus_device_parser_apply(&parser_packet, parse_context, &record->application);
}

void wmbus_packet_finalize_parser(WmBusPacketRecord* record) {
    if(!record) return;

    if(record->application.parser_id == WmBusParserIdUnknown) {
        record->application.parser_id = record->tpl.has_short_tpl ? WmBusParserIdShortTpl :
                                                                     (record->packet_is_frame ?
                                                                          WmBusParserIdHeader :
                                                                          WmBusParserIdRaw);
    }
}
