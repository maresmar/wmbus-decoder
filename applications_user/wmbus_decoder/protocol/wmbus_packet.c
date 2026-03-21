#include "wmbus_packet.h"

#include <furi.h>
#include <stdio.h>
#include <string.h>

#include "frame/wmbus_frame.h"
#include "parser/wmbus_device_parser.h"
#include "parser/wmbus_parser.h"

#define TAG                 "WmBusDecoder"
#define WMBUS_DECODE_MAX    256U
#define WMBUS_MODE5_KEY_LEN 16U

typedef struct {
    bool decoded_ok;
    bool plausible;
    bool length_ok;
    bool crc_known;
    bool crc_ok;
    size_t frame_len;
    int best_offset;
    uint8_t frame[WMBUS_DECODE_MAX];
} WmBusTDecodeResult;

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
    return record->dll.id_str[0] ? record->dll.id_str : "--------";
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

static const char* wmbus_packet_security_mode_name(uint8_t security_mode) {
    switch(security_mode) {
    case 0x00:
        return "Clear";
    case 0x01:
        return "Manufacturer";
    case 0x05:
        return "AES-CBC IV";
    case 0x08:
        return "AES-CTR CMAC";
    default:
        return NULL;
    }
}

static void wmbus_packet_populate_application_identity(WmBusPacketRecord* record) {
    if(!record) return;

    wmbus_frame_decode_mfg(record->dll.m_field, record->dll.mfg);
    wmbus_frame_format_id(record->dll.id, record->dll.id_str, &record->dll.id_is_bcd);
}

const char* wmbus_packet_status_str(WmBusStatus status) {
    switch(status) {
    case WmBusStatusDecodeFail:
        return "Decode fail";
    case WmBusStatusNotPlausible:
        return "Not plausible";
    case WmBusStatusFramingError:
        return "Framing error";
    case WmBusStatusCrcBad:
        return "CRC bad";
    case WmBusStatusWeakRssi:
        return "Weak RSSI";
    case WmBusStatusOk:
        return "OK";
    default:
        return "--";
    }
}

const char* wmbus_packet_status_short_label(WmBusStatus status) {
    switch(status) {
    case WmBusStatusDecodeFail:
        return "Decode";
    case WmBusStatusNotPlausible:
        return "Plausible";
    case WmBusStatusFramingError:
        return "Framing";
    case WmBusStatusCrcBad:
        return "CRC";
    case WmBusStatusWeakRssi:
        return "Weak RSSI";
    case WmBusStatusOk:
        return "OK";
    default:
        return "--";
    }
}

const char* wmbus_packet_csv_logging_str(WmBusCsvLogging logging) {
    switch(logging) {
    case WmBusCsvLoggingBasic:
        return "Basic";
    case WmBusCsvLoggingFull:
        return "Full";
    case WmBusCsvLoggingNone:
    default:
        return "None";
    }
}

void wmbus_packet_format_total_m3(uint32_t total_m3_x1000, char* out, size_t out_size) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';

    uint32_t whole = total_m3_x1000 / 1000U;
    uint32_t frac = total_m3_x1000 % 1000U;
    snprintf(out, out_size, "%lu.%03lu m3", (unsigned long)whole, (unsigned long)frac);
}

void wmbus_packet_format_security_summary(
    bool has_short_tpl,
    uint8_t security_mode,
    bool security_likely_encrypted,
    bool decrypted,
    uint8_t key_index,
    char* out,
    size_t out_size) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';
    if(!has_short_tpl) return;

    if(decrypted) {
        if(key_index != 0U) {
            snprintf(out, out_size, "Dec #%u", (unsigned int)key_index);
        } else {
            snprintf(out, out_size, "Dec zero");
        }
        return;
    }

    if(security_likely_encrypted) {
        snprintf(out, out_size, "Enc");
        return;
    }

    const char* known_mode = wmbus_packet_security_mode_name(security_mode);
    if(known_mode) {
        if(security_mode == 0x01U) {
            snprintf(out, out_size, "Mfg");
        } else {
            snprintf(out, out_size, "%s", known_mode);
        }
    } else {
        snprintf(out, out_size, "Mode %02X", security_mode);
    }
}

void wmbus_packet_format_security_text(
    bool has_short_tpl,
    uint8_t security_mode,
    bool security_likely_encrypted,
    bool decrypted,
    uint8_t key_index,
    char* out,
    size_t out_size) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';
    if(!has_short_tpl) return;

    char mode[20] = {0};
    const char* known_mode = wmbus_packet_security_mode_name(security_mode);
    if(known_mode) {
        snprintf(mode, sizeof(mode), "%s", known_mode);
    } else {
        snprintf(mode, sizeof(mode), "Mode %02X", security_mode);
    }

    if(decrypted) {
        if(key_index != 0U) {
            snprintf(out, out_size, "%s, decrypted key #%u", mode, (unsigned int)key_index);
        } else {
            snprintf(out, out_size, "%s, decrypted zero key", mode);
        }
    } else if(security_likely_encrypted) {
        snprintf(out, out_size, "%s, encrypted", mode);
    } else {
        snprintf(out, out_size, "%s", mode);
    }
}

static bool wmbus_ci_has_short_tpl(uint8_t ci) {
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
    wmbus_packet_populate_application_identity(record);
    record->tpl.header_len = 11U;
    record->tpl.security_mode = 0U;

    if(frame_len >= 15U && wmbus_ci_has_short_tpl(frame[10])) {
        record->tpl.has_short_tpl = true;
        record->tpl.header_len = 15U;
        record->tpl.acc = frame[11];
        record->tpl.tpl_status = frame[12];
        record->tpl.cfg = (uint16_t)frame[13] | ((uint16_t)frame[14] << 8);
        record->tpl.security_mode = wmbus_parser_short_tpl_security_mode(record->tpl.cfg);
    }
}

static void wmbus_packet_set_payload_packet_slice(
    WmBusPacketRecord* record, size_t frame_len) {
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

static void wmbus_packet_set_application_payload(
    WmBusPacketRecord* record,
    const uint8_t* frame,
    size_t frame_len) {
    if(!record || !frame) return;

    size_t offset = record->tpl.header_len;
    if(frame_len <= offset) {
        record->payload.has_application_payload = false;
        record->payload.application_len = 0U;
        return;
    }

    size_t payload_len = frame_len - offset;
    if(payload_len > sizeof(record->payload.application_payload)) {
        payload_len = sizeof(record->payload.application_payload);
    }

    record->payload.has_application_payload = true;
    record->payload.application_len = (uint16_t)payload_len;
    memcpy(record->payload.application_payload, &frame[offset], payload_len);
}

static bool wmbus_packet_try_key(
    const uint8_t* frame,
    size_t frame_len,
    const uint8_t key[WMBUS_MODE5_KEY_LEN],
    WmBusPacketRecord* record,
    uint8_t decrypt_frame[WMBUS_DECODE_MAX]) {
    if(!frame || !key || !record || !decrypt_frame) {
        return false;
    }

    WmBusMode5DecryptInfo decrypt =
        wmbus_parser_decrypt_mode5(frame, frame_len, record->tpl.cfg, key, decrypt_frame);
    if(decrypt.result != WmBusDecryptResultOk) {
        wmbus_packet_log_decrypt_failure(record, key, decrypt.result);
        return false;
    }

    wmbus_packet_set_application_payload(record, decrypt_frame, frame_len);
    bool parsed = wmbus_device_parser_apply(record);

    if(parsed || decrypt.has_check_bytes) {
        return true;
    }

    wmbus_packet_log_decrypt_failure_reason(record, key, "missing 2F2F check bytes");
    return false;
}

static bool wmbus_packet_select_parse_frame(
    const uint8_t* frame,
    size_t frame_len,
    WmBusPacketRecord* record,
    const WmBusKeyring* keyring) {
    static const uint8_t wmbus_zero_key[WMBUS_MODE5_KEY_LEN] = {0};

    if(!frame || !record) {
        return false;
    }

    record->tpl.decrypted = false;
    record->tpl.key_index = 0U;
    memset(&record->application, 0, sizeof(record->application));
    wmbus_packet_populate_application_identity(record);

    wmbus_packet_set_payload_packet_slice(record, frame_len);

    // Some Apator telegrams advertise mode-5 settings in short TPL but still
    // carry a clear application payload prefixed with 2F2F. Treat those as
    // already clear instead of forcing a zero-key decrypt attempt.
    if(record->payload.packet_len >= 2U &&
       record->packet_bytes[record->payload.packet_offset] == 0x2FU &&
       record->packet_bytes[record->payload.packet_offset + 1U] == 0x2FU) {
        wmbus_packet_set_application_payload(record, frame, frame_len);
        return wmbus_device_parser_apply(record);
    }

    if(!record->tpl.has_short_tpl ||
       !wmbus_parser_short_tpl_security_likely_encrypted(record->tpl.cfg)) {
        wmbus_packet_set_application_payload(record, frame, frame_len);
        return wmbus_device_parser_apply(record);
    }

    uint8_t security_mode = record->tpl.security_mode;
    if(security_mode != 0x05U) {
        FURI_LOG_D(
            TAG,
            "decrypt skipped id=%s ci=%02X cfg=%04X unsupported security mode=%02X",
            wmbus_packet_log_id(record),
            record->dll.ci_field,
            record->tpl.cfg,
            security_mode);
        return false;
    }

    uint8_t decrypt_frame[WMBUS_DECODE_MAX] = {0};

    for(uint8_t i = 0; keyring && i < keyring->count; i++) {
        const WmBusKeyEntry* entry = wmbus_keyring_get(keyring, i);
        if(!entry) continue;

        if(wmbus_packet_try_key(frame, frame_len, entry->key, record, decrypt_frame)) {
            record->tpl.decrypted = true;
            record->tpl.key_index = i + 1U;
            return true;
        }
    }

    if(wmbus_packet_try_key(frame, frame_len, wmbus_zero_key, record, decrypt_frame)) {
        record->tpl.decrypted = true;
        record->tpl.key_index = 0U;
        return true;
    }

    memset(&record->application, 0, sizeof(record->application));
    wmbus_packet_populate_application_identity(record);
    wmbus_packet_set_application_payload(record, frame, frame_len);
    return wmbus_device_parser_apply(record);
}

static void wmbus_packet_finalize_parser(WmBusPacketRecord* record) {
    if(!record) return;

    if(record->application.parser_name[0] == '\0') {
        snprintf(
            record->application.parser_name,
            sizeof(record->application.parser_name),
            "%s",
            record->tpl.has_short_tpl ? "Short TPL" :
                                              (record->packet_is_frame ? "Header" : "Raw"));
    }
}

static int wmbus_score_t_decode_candidate(const WmBusTDecodeResult* candidate) {
    int score = 0;
    if(candidate->decoded_ok) score += 1;
    if(candidate->plausible) score += 4;
    if(candidate->length_ok) score += 2;
    if(candidate->crc_ok) score += 1;
    return score;
}

static bool wmbus_try_decode_t_candidate(
    const WmBusCaptureFrame* capture,
    uint8_t bit_offset,
    uint8_t tail_pad,
    WmBusTDecodeResult* result) {
    if(!capture || !result) return false;

    size_t raw_bit_len = capture->len * 8U;
    if(raw_bit_len <= tail_pad) return false;
    raw_bit_len -= tail_pad;

    memset(result, 0, sizeof(*result));
    result->best_offset = bit_offset;

    uint8_t decoded[WMBUS_DECODE_MAX] = {0};
    size_t decoded_len = 0;
    if(!wmbus_parser_decode_3of6_bits(
           capture->data, raw_bit_len, bit_offset, decoded, sizeof(decoded), &decoded_len)) {
        return false;
    }

    result->decoded_ok = true;
    result->plausible = wmbus_parser_is_plausible(decoded, decoded_len);
    if(!result->plausible) return true;

    const uint8_t* frame = decoded;
    size_t frame_len = decoded_len;
    uint8_t normalized[WMBUS_DECODE_MAX] = {0};
    WmBusFrameNormalizeResult normalized_result = {0};
    if(wmbus_frame_normalize(
           WmBusRxModeT, decoded, decoded_len, normalized, sizeof(normalized), &normalized_result)) {
        frame = normalized;
        frame_len = normalized_result.normalized_len;
    }

    result->length_ok = normalized_result.length_ok;
    result->crc_known = normalized_result.crc_known;
    result->crc_ok = normalized_result.crc_ok;
    result->frame_len = frame_len;
    memcpy(result->frame, frame, frame_len);
    return true;
}

static void wmbus_decode_t_capture(const WmBusCaptureFrame* capture, WmBusTDecodeResult* result) {
    if(!capture || !result) return;

    memset(result, 0, sizeof(*result));
    result->best_offset = -1;

    int best_score = -1;
    uint8_t best_tail_pad = 0xFFU;

    for(uint8_t bit_offset = 0; bit_offset < 8U; bit_offset++) {
        for(uint8_t tail_pad = 0; tail_pad < 8U; tail_pad++) {
            WmBusTDecodeResult candidate = {0};
            if(!wmbus_try_decode_t_candidate(capture, bit_offset, tail_pad, &candidate)) {
                continue;
            }

            int score = wmbus_score_t_decode_candidate(&candidate);
            bool better = false;
            if(score > best_score) {
                better = true;
            } else if(score == best_score && score >= 0) {
                if(result->best_offset < 0 || candidate.best_offset < result->best_offset) {
                    better = true;
                } else if(candidate.best_offset == result->best_offset && tail_pad < best_tail_pad) {
                    better = true;
                }
            }

            if(better) {
                *result = candidate;
                best_score = score;
                best_tail_pad = tail_pad;
            }
        }
    }
}

bool wmbus_packet_process_capture(
    const WmBusCaptureFrame* capture,
    const WmBusKeyring* keyring,
    WmBusPacketRecord* record) {
    if(!capture || !record) return false;

    memset(record, 0, sizeof(*record));
    record->mode = capture->mode;
    record->raw_len = (uint16_t)capture->raw_len;
    record->best_offset = -1;
    record->rssi = capture->rssi;
    record->rx_tick = furi_get_tick();

    const uint8_t* frame = NULL;
    size_t frame_len = 0;
    bool decoded_ok = false;
    bool plausible = false;
    bool length_ok = false;
    bool crc_known = false;
    bool crc_ok = false;
    bool used_3of6 = (capture->mode == WmBusRxModeT);

    if(used_3of6) {
        WmBusTDecodeResult t_result = {0};
        wmbus_decode_t_capture(capture, &t_result);
        decoded_ok = t_result.decoded_ok;
        plausible = t_result.plausible;
        length_ok = t_result.length_ok;
        crc_known = t_result.crc_known;
        crc_ok = t_result.crc_ok;
        record->best_offset = t_result.best_offset;

        if(plausible) {
            frame = t_result.frame;
            frame_len = t_result.frame_len;
        }
    } else {
        decoded_ok = true;
        if(wmbus_parser_is_plausible(capture->data, capture->len)) {
            plausible = true;
            frame = capture->data;
            frame_len = capture->len;
        }
    }

    uint8_t normalized[WMBUS_DECODE_MAX] = {0};
    if(plausible && !used_3of6) {
        WmBusFrameNormalizeResult normalized_result = {0};
        if(wmbus_frame_normalize(
               capture->mode,
               frame,
               frame_len,
               normalized,
               sizeof(normalized),
               &normalized_result)) {
            frame = normalized;
            frame_len = normalized_result.normalized_len;
        }
        length_ok = normalized_result.length_ok;
        crc_known = normalized_result.crc_known;
        crc_ok = normalized_result.crc_ok;
    }

    record->decoded_ok = decoded_ok;
    record->plausible = plausible;
    record->length_ok = length_ok;
    record->crc_known = crc_known;
    record->crc_ok = crc_ok;
    record->strong_rssi = (capture->rssi >= -70);

    if(plausible && frame && frame_len > 0U) {
        record->packet_is_frame = true;
        record->packet_len = (uint16_t)((frame_len > sizeof(record->packet_bytes)) ?
                                            sizeof(record->packet_bytes) :
                                            frame_len);
        memcpy(record->packet_bytes, frame, record->packet_len);
        wmbus_packet_extract_dll_tpl_info(frame, frame_len, record);

        wmbus_packet_select_parse_frame(frame, frame_len, record, keyring);
        wmbus_packet_finalize_parser(record);
    } else {
        record->packet_is_frame = false;
        record->packet_len = (uint16_t)((capture->len > sizeof(record->packet_bytes)) ?
                                            sizeof(record->packet_bytes) :
                                            capture->len);
        memcpy(record->packet_bytes, capture->data, record->packet_len);
        snprintf(record->application.parser_name, sizeof(record->application.parser_name), "Raw");
    }

    if(used_3of6 && !decoded_ok) {
        record->status = WmBusStatusDecodeFail;
    } else if(!plausible) {
        record->status = WmBusStatusNotPlausible;
    } else if(!length_ok) {
        record->status = WmBusStatusFramingError;
    } else if(crc_known && !crc_ok) {
        record->status = WmBusStatusCrcBad;
    } else if(!record->strong_rssi) {
        record->status = WmBusStatusWeakRssi;
    } else {
        record->status = WmBusStatusOk;
    }

    return true;
}
