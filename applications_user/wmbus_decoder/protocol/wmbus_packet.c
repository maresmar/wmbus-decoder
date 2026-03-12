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

typedef struct {
    bool decrypted;
    bool key_applied;
    uint8_t key_index;
    bool parser_applied;
} WmBusPacketParseSelection;

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
    return record->data.id_str[0] ? record->data.id_str : "--------";
}

static void wmbus_packet_log_decrypt_failure_reason(
    const WmBusPacketRecord* record,
    const char* key_label,
    const char* reason) {
    if(!record || !key_label || !reason || reason[0] == '\0') return;

    FURI_LOG_D(
        TAG,
        "decrypt failed id=%s ci=%02X cfg=%04X key=%s reason=%s",
        wmbus_packet_log_id(record),
        record->data.ci_field,
        record->data.cfg,
        key_label,
        reason);
}

static void wmbus_packet_log_decrypt_failure(
    const WmBusPacketRecord* record,
    const char* key_label,
    WmBusDecryptResult result) {
    if(!record || !key_label || result == WmBusDecryptResultOk) return;

    wmbus_packet_log_decrypt_failure_reason(
        record, key_label, wmbus_packet_decrypt_result_str(result));
}

static void
    wmbus_packet_add_field(WmBusPacketRecord* record, const char* label, const char* value) {
    if(!record || !label || !value) return;
    if(record->data.field_count >= WMBUS_PACKET_FIELD_MAX) return;

    WmBusPacketField* field = &record->data.fields[record->data.field_count++];
    snprintf(field->label, sizeof(field->label), "%s", label);
    snprintf(field->value, sizeof(field->value), "%s", value);
}

static void wmbus_packet_set_primary(
    WmBusPacketRecord* record,
    const char* primary_a,
    const char* primary_b) {
    if(!record) return;

    if(primary_a) {
        snprintf(record->data.primary_a, sizeof(record->data.primary_a), "%s", primary_a);
    }
    if(primary_b) {
        snprintf(record->data.primary_b, sizeof(record->data.primary_b), "%s", primary_b);
    }
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

static bool wmbus_packet_parser_name_is_generic(const char* parser_name) {
    if(!parser_name || parser_name[0] == '\0') return true;

    return strcmp(parser_name, "Short TPL") == 0 || strcmp(parser_name, "Header") == 0 ||
           strcmp(parser_name, "Raw") == 0;
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
    bool key_applied,
    uint8_t key_index,
    char* out,
    size_t out_size) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';
    if(!has_short_tpl) return;

    if(decrypted) {
        if(key_applied) {
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
    bool key_applied,
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
        if(key_applied) {
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

static void wmbus_packet_extract_frame_info(
    const uint8_t* frame,
    size_t frame_len,
    WmBusPacketRecord* record) {
    if(!frame || !record || frame_len < 11U) return;

    record->data.l_field = frame[0];
    record->data.c_field = frame[1];
    record->data.m_field = (uint16_t)frame[2] | ((uint16_t)frame[3] << 8);
    wmbus_frame_decode_mfg(record->data.m_field, record->data.mfg);
    memcpy(record->data.id, &frame[4], sizeof(record->data.id));
    wmbus_frame_format_id(record->data.id, record->data.id_str, &record->data.id_is_bcd);
    record->data.version = frame[8];
    record->data.dev_type = frame[9];
    record->data.ci_field = frame[10];

    if(frame_len >= 15U && wmbus_ci_has_short_tpl(frame[10])) {
        record->data.has_short_tpl = true;
        record->data.acc = frame[11];
        record->data.tpl_status = frame[12];
        record->data.cfg = (uint16_t)frame[13] | ((uint16_t)frame[14] << 8);
        record->data.security_mode = wmbus_parser_short_tpl_security_mode(record->data.cfg);
        record->data.security_likely_encrypted =
            wmbus_parser_short_tpl_security_likely_encrypted(record->data.cfg);
    }
}

static void wmbus_packet_add_short_tpl_fields(WmBusPacketRecord* record) {
    if(!record || !record->data.has_short_tpl) return;

    char temp[WMBUS_PACKET_VALUE_MAX];

    snprintf(temp, sizeof(temp), "%02X", record->data.acc);
    wmbus_packet_add_field(record, "ACC", temp);

    snprintf(temp, sizeof(temp), "%02X", record->data.tpl_status);
    wmbus_packet_add_field(record, "TPL", temp);

    snprintf(temp, sizeof(temp), "%04X", record->data.cfg);
    wmbus_packet_add_field(record, "CFG", temp);

    snprintf(temp, sizeof(temp), "%02X", record->data.security_mode);
    wmbus_packet_add_field(record, "SEC", temp);

    if(record->data.decrypted) {
        if(record->data.key_applied) {
            snprintf(temp, sizeof(temp), "#%u", (unsigned int)record->data.key_index);
        } else {
            snprintf(temp, sizeof(temp), "zero");
        }
        wmbus_packet_add_field(record, "Key", temp);
    } else if(record->data.security_likely_encrypted) {
        wmbus_packet_add_field(record, "Payload", "Encrypted");
    }
}

static bool wmbus_packet_try_parser_candidate(
    const uint8_t* candidate_frame,
    size_t frame_len,
    bool decrypted,
    bool key_applied,
    uint8_t key_index,
    WmBusPacketRecord* record,
    WmBusPacketParseSelection* selection) {
    if(!candidate_frame || !record || !selection) {
        return false;
    }
    if(!wmbus_device_parser_apply(candidate_frame, frame_len, record)) {
        return false;
    }

    selection->decrypted = decrypted;
    selection->key_applied = key_applied;
    selection->key_index = key_index;
    selection->parser_applied = true;
    return true;
}

static void wmbus_packet_select_parse_frame(
    const uint8_t* frame,
    size_t frame_len,
    WmBusPacketRecord* record,
    const WmBusKeyring* keyring,
    WmBusPacketParseSelection* selection) {
    static const uint8_t wmbus_zero_key[WMBUS_MODE5_KEY_LEN] = {0};

    if(!frame || !record || !selection) {
        return;
    }

    *selection = (WmBusPacketParseSelection){
        .decrypted = false,
        .key_applied = false,
        .key_index = 0U,
        .parser_applied = false,
    };

    bool have_fallback = false;
    WmBusPacketParseSelection fallback = *selection;
    if(wmbus_parser_short_tpl_payload_has_check_bytes(frame, frame_len)) {
        have_fallback = true;
    }

    if(wmbus_packet_try_parser_candidate(frame, frame_len, false, false, 0U, record, selection)) {
        return;
    }

    if(!record->data.has_short_tpl || !record->data.security_likely_encrypted) {
        return;
    }

    if(record->data.security_mode != 0x05U) {
        FURI_LOG_D(
            TAG,
            "decrypt skipped id=%s ci=%02X cfg=%04X unsupported security mode=%02X",
            wmbus_packet_log_id(record),
            record->data.ci_field,
            record->data.cfg,
            record->data.security_mode);
        return;
    }

    if(have_fallback) {
        return;
    }

    uint8_t decrypt_frame[WMBUS_DECODE_MAX] = {0};

    for(uint8_t i = 0; keyring && i < keyring->count; i++) {
        const WmBusKeyEntry* entry = wmbus_keyring_get(keyring, i);
        if(!entry) continue;

        char key_label[8] = {0};
        snprintf(key_label, sizeof(key_label), "#%u", (unsigned int)(i + 1U));

        WmBusMode5DecryptInfo decrypt =
            wmbus_parser_decrypt_mode5(frame, frame_len, record->data.cfg, entry->key, decrypt_frame);
        if(decrypt.result != WmBusDecryptResultOk) {
            wmbus_packet_log_decrypt_failure(record, key_label, decrypt.result);
            continue;
        }

        if(wmbus_packet_try_parser_candidate(
               decrypt_frame,
               frame_len,
               true,
               true,
               (uint8_t)(i + 1U),
               record,
               selection)) {
            return;
        }

        if(decrypt.has_check_bytes && !have_fallback) {
            fallback = (WmBusPacketParseSelection){
                .decrypted = true,
                .key_applied = true,
                .key_index = (uint8_t)(i + 1U),
                .parser_applied = false,
            };
            have_fallback = true;
        } else if(!decrypt.has_check_bytes) {
            wmbus_packet_log_decrypt_failure_reason(record, key_label, "missing 2F2F check bytes");
        }
    }

    WmBusMode5DecryptInfo zero_decrypt =
        wmbus_parser_decrypt_mode5(frame, frame_len, record->data.cfg, wmbus_zero_key, decrypt_frame);
    if(zero_decrypt.result != WmBusDecryptResultOk) {
        wmbus_packet_log_decrypt_failure(record, "zero", zero_decrypt.result);
    } else if(wmbus_packet_try_parser_candidate(
                  decrypt_frame,
                  frame_len,
                  true,
                  false,
                  0U,
                  record,
                  selection)) {
        return;
    } else if(zero_decrypt.has_check_bytes && !have_fallback) {
        fallback = (WmBusPacketParseSelection){
            .decrypted = true,
            .key_applied = false,
            .key_index = 0U,
            .parser_applied = false,
        };
        have_fallback = true;
    } else if(!zero_decrypt.has_check_bytes) {
        wmbus_packet_log_decrypt_failure_reason(record, "zero", "missing 2F2F check bytes");
    }

    if(have_fallback) {
        *selection = fallback;
    }
}

static void wmbus_packet_finalize_parser(bool parsed, WmBusPacketRecord* record) {
    if(!record) return;

    if(!parsed) {
        snprintf(
            record->data.parser_name,
            sizeof(record->data.parser_name),
            "%s",
            record->data.has_short_tpl ? "Short TPL" : "Raw");

        wmbus_packet_add_short_tpl_fields(record);
    }

    if(record->data.primary_a[0] == '\0') {
        char primary_a[WMBUS_PACKET_VALUE_MAX] = {0};
        char primary_b[WMBUS_PACKET_VALUE_MAX] = {0};

        if(record->data.has_short_tpl) {
            wmbus_packet_format_security_summary(
                record->data.has_short_tpl,
                record->data.security_mode,
                record->data.security_likely_encrypted,
                record->data.decrypted,
                record->data.key_applied,
                record->data.key_index,
                primary_a,
                sizeof(primary_a));
            if(primary_a[0] == '\0') {
                snprintf(primary_a, sizeof(primary_a), "CI:%02X", record->data.ci_field);
            }
            if(record->data.ci_field != 0U) {
                snprintf(primary_b, sizeof(primary_b), "CI:%02X", record->data.ci_field);
            }
        } else if(record->packet_is_frame) {
            snprintf(primary_a, sizeof(primary_a), "CI:%02X", record->data.ci_field);
            snprintf(primary_b, sizeof(primary_b), "R:%d", record->rssi);
        } else {
            snprintf(
                primary_a, sizeof(primary_a), "Len:%u bytes", (unsigned int)record->packet_len);
        }
        wmbus_packet_set_primary(record, primary_a, primary_b[0] ? primary_b : NULL);
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
        wmbus_packet_extract_frame_info(frame, frame_len, record);

        WmBusPacketParseSelection selection = {0};
        wmbus_packet_select_parse_frame(frame, frame_len, record, keyring, &selection);
        record->data.decrypted = selection.decrypted;
        record->data.key_applied = selection.key_applied;
        record->data.key_index = selection.key_index;
        wmbus_packet_finalize_parser(selection.parser_applied, record);
    } else {
        record->packet_is_frame = false;
        record->packet_len = (uint16_t)((capture->len > sizeof(record->packet_bytes)) ?
                                            sizeof(record->packet_bytes) :
                                            capture->len);
        memcpy(record->packet_bytes, capture->data, record->packet_len);
        snprintf(record->data.parser_name, sizeof(record->data.parser_name), "Raw");
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

void wmbus_packet_build_fields_text(const WmBusPacketRecord* record, char* out, size_t out_size) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';
    if(!record) return;

    size_t write = 0;
    for(uint8_t i = 0; i < record->data.field_count; i++) {
        const WmBusPacketField* field = &record->data.fields[i];
        int len = snprintf(
            &out[write],
            out_size - write,
            "%s%s=%s",
            (i == 0U) ? "" : ";",
            field->label,
            field->value);
        if(len < 0) break;
        if((size_t)len >= (out_size - write)) {
            write = out_size - 1U;
            break;
        }
        write += (size_t)len;
    }
}

void wmbus_packet_build_detail_text(const WmBusPacketRecord* record, char* out, size_t out_size) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';
    if(!record) return;

    char fields[384] = {0};
    char total[WMBUS_PACKET_VALUE_MAX] = {0};
    char security[48] = {0};
    char summary_a[48] = {0};
    char summary_b[64] = {0};
    char detail_fields[416] = {0};
    char parser_line[40] = {0};
    char mode_line[32] = {0};
    const char* detail_tail = "-";
    bool prefer_parser_primary = !wmbus_packet_parser_name_is_generic(record->data.parser_name);
    wmbus_packet_build_fields_text(record, fields, sizeof(fields));
    if(record->data.has_total_m3) {
        wmbus_packet_format_total_m3(record->data.total_m3_x1000, total, sizeof(total));
    }
    wmbus_packet_format_security_text(
        record->data.has_short_tpl,
        record->data.security_mode,
        record->data.security_likely_encrypted,
        record->data.decrypted,
        record->data.key_applied,
        record->data.key_index,
        security,
        sizeof(security));
    if(total[0]) {
        snprintf(summary_a, sizeof(summary_a), "Total: %s\n", total);
    } else if(record->data.primary_a[0] && prefer_parser_primary) {
        snprintf(summary_a, sizeof(summary_a), "%s\n", record->data.primary_a);
    }
    if(security[0]) {
        snprintf(summary_b, sizeof(summary_b), "Security: %s\n", security);
    } else if(record->data.primary_b[0] && prefer_parser_primary) {
        snprintf(summary_b, sizeof(summary_b), "%s\n", record->data.primary_b);
    }
    if(fields[0] && !record->data.has_total_m3 && security[0] == '\0') {
        snprintf(detail_fields, sizeof(detail_fields), "Fields: %s", fields);
    }
    if(detail_fields[0]) {
        detail_tail = detail_fields;
    } else if(summary_a[0] || summary_b[0]) {
        detail_tail = "";
    }

    snprintf(
        mode_line,
        sizeof(mode_line),
        "M:%c  R:%d",
        record->mode == WmBusRxModeT ? 'T' : 'C',
        record->rssi);
    if(prefer_parser_primary) {
        snprintf(parser_line, sizeof(parser_line), "Parser: %s\n", record->data.parser_name);
    }

    if(record->packet_is_frame) {
        snprintf(
            out,
            out_size,
            "Status: %s\nMF:%s  DT:%02X  ID:%s\n%s\nCI:%02X  V:%02X\n%s%s%s%s",
            wmbus_packet_status_str(record->status),
            record->data.mfg,
            record->data.dev_type,
            record->data.id_str,
            mode_line,
            record->data.ci_field,
            record->data.version,
            parser_line,
            summary_a,
            summary_b,
            detail_tail);
    } else {
        snprintf(
            out,
            out_size,
            "Status: %s\nMode: %c  RSSI: %d\nParser: %s\nLen: %u bytes",
            wmbus_packet_status_str(record->status),
            record->mode == WmBusRxModeT ? 'T' : 'C',
            record->rssi,
            record->data.parser_name,
            (unsigned int)record->packet_len);
    }
}
