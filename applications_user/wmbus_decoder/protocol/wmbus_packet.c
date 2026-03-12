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
    return record->frame.id_str[0] ? record->frame.id_str : "--------";
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
        record->frame.ci_field,
        record->transport.cfg,
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

static void
    wmbus_packet_add_field(WmBusPacketRecord* record, const char* label, const char* value) {
    if(!record || !label || !value) return;
    if(record->application.field_count >= WMBUS_PACKET_FIELD_MAX) return;

    WmBusPacketField* field = &record->application.fields[record->application.field_count++];
    snprintf(field->label, sizeof(field->label), "%s", label);
    snprintf(field->value, sizeof(field->value), "%s", value);
}

static void wmbus_packet_set_summary(
    WmBusPacketRecord* record,
    const char* summary_a,
    const char* summary_b) {
    if(!record) return;

    if(summary_a) {
        snprintf(record->application.summary_a, sizeof(record->application.summary_a), "%s", summary_a);
    }
    if(summary_b) {
        snprintf(record->application.summary_b, sizeof(record->application.summary_b), "%s", summary_b);
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
           strcmp(parser_name, "Raw") == 0 || strcmp(parser_name, "DIF/VIF") == 0;
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

static uint64_t wmbus_packet_pow10_u64(uint8_t power) {
    uint64_t result = 1U;
    while(power > 0U) {
        result *= 10U;
        power--;
    }
    return result;
}

static void
    wmbus_packet_format_scaled_unsigned(uint64_t value, int8_t scale10, char* out, size_t out_size) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';

    if(scale10 >= 0) {
        uint64_t scaled = value * wmbus_packet_pow10_u64((uint8_t)scale10);
        snprintf(out, out_size, "%llu", (unsigned long long)scaled);
        return;
    }

    uint8_t decimals = (uint8_t)(-scale10);
    uint64_t div = wmbus_packet_pow10_u64(decimals);
    uint64_t whole = value / div;
    uint64_t frac = value % div;
    snprintf(
        out,
        out_size,
        "%llu.%0*llu",
        (unsigned long long)whole,
        (int)decimals,
        (unsigned long long)frac);
}

static bool wmbus_packet_decode_unsigned_le(const uint8_t* data, uint8_t data_len, uint64_t* value) {
    if(!data || !value || data_len == 0U || data_len > 8U) return false;

    uint64_t result = 0U;
    for(uint8_t i = 0; i < data_len; i++) {
        result |= ((uint64_t)data[i]) << (8U * i);
    }
    *value = result;
    return true;
}

static bool wmbus_packet_decode_bcd(const uint8_t* data, uint8_t data_len, uint64_t* value) {
    if(!data || !value || data_len == 0U || data_len > 8U) return false;

    uint64_t result = 0U;
    uint64_t factor = 1U;
    for(uint8_t i = 0; i < data_len; i++) {
        uint8_t lo = data[i] & 0x0FU;
        uint8_t hi = (data[i] >> 4) & 0x0FU;
        if(lo > 9U || hi > 9U) return false;
        result += (uint64_t)lo * factor;
        factor *= 10U;
        result += (uint64_t)hi * factor;
        factor *= 10U;
    }
    *value = result;
    return true;
}

static int wmbus_packet_data_len_from_dif(uint8_t dif, bool* is_bcd, bool* is_variable_text) {
    if(is_bcd) *is_bcd = false;
    if(is_variable_text) *is_variable_text = false;

    switch(dif & 0x0FU) {
    case 0x00:
        return 0;
    case 0x01:
        return 1;
    case 0x02:
        return 2;
    case 0x03:
        return 3;
    case 0x04:
        return 4;
    case 0x05:
        return 4;
    case 0x06:
        return 6;
    case 0x07:
        return 8;
    case 0x09:
        if(is_bcd) *is_bcd = true;
        return 1;
    case 0x0A:
        if(is_bcd) *is_bcd = true;
        return 2;
    case 0x0B:
        if(is_bcd) *is_bcd = true;
        return 3;
    case 0x0C:
        if(is_bcd) *is_bcd = true;
        return 4;
    case 0x0D:
        if(is_variable_text) *is_variable_text = true;
        return -2;
    default:
        return -1;
    }
}

static void wmbus_packet_map_vif(WmBusApplicationRecord* record) {
    if(!record) return;

    snprintf(record->label, sizeof(record->label), "Data");
    record->unit[0] = '\0';
    record->quantity = WmBusApplicationQuantityUnknown;
    record->scale10 = 0;

    if((record->vif & 0xF8U) == 0x10U) {
        record->quantity = WmBusApplicationQuantityVolume;
        record->scale10 = (int8_t)(record->vif & 0x07U) - 6;
        snprintf(record->label, sizeof(record->label), "Volume");
        snprintf(record->unit, sizeof(record->unit), "m3");
    } else if((record->vif & 0xF8U) == 0x00U) {
        record->quantity = WmBusApplicationQuantityEnergy;
        record->scale10 = (int8_t)(record->vif & 0x07U) - 3;
        snprintf(record->label, sizeof(record->label), "Energy");
        snprintf(record->unit, sizeof(record->unit), "Wh");
    }
}

static void wmbus_packet_decode_record_value(
    WmBusApplicationRecord* record,
    const uint8_t* data,
    uint8_t data_len,
    bool is_bcd,
    bool is_variable_text) {
    if(!record) return;

    record->value_type = WmBusApplicationValueNone;
    record->value_text[0] = '\0';
    record->value_unsigned = 0U;

    if(data_len == 0U) {
        snprintf(record->value_text, sizeof(record->value_text), "-");
        return;
    }

    if(is_variable_text) {
        record->value_type = WmBusApplicationValueText;
        size_t write = 0U;
        for(uint8_t i = 0; i < data_len && (write + 2U) < sizeof(record->value_text); i++) {
            snprintf(
                &record->value_text[write],
                sizeof(record->value_text) - write,
                "%02X",
                data[i]);
            write += 2U;
        }
        return;
    }

    bool decoded = false;
    uint64_t value = 0U;
    if(is_bcd) {
        decoded = wmbus_packet_decode_bcd(data, data_len, &value);
    } else if((data_len <= 8U) && ((record->dif & 0x0FU) != 0x05U)) {
        decoded = wmbus_packet_decode_unsigned_le(data, data_len, &value);
    }

    if(!decoded) {
        record->value_type = WmBusApplicationValueText;
        size_t write = 0U;
        for(uint8_t i = 0; i < data_len && (write + 2U) < sizeof(record->value_text); i++) {
            snprintf(
                &record->value_text[write],
                sizeof(record->value_text) - write,
                "%02X",
                data[i]);
            write += 2U;
        }
        return;
    }

    record->value_type = WmBusApplicationValueUnsigned;
    record->value_unsigned = value;
    wmbus_packet_format_scaled_unsigned(value, record->scale10, record->value_text, sizeof(record->value_text));
    if(record->unit[0] != '\0') {
        size_t len = strlen(record->value_text);
        snprintf(
            &record->value_text[len],
            sizeof(record->value_text) - len,
            " %s",
            record->unit);
    }
}

static bool wmbus_packet_total_volume_from_record(
    const WmBusApplicationRecord* app_record,
    uint32_t* total_m3_x1000) {
    if(!app_record || !total_m3_x1000) return false;
    if(app_record->quantity != WmBusApplicationQuantityVolume ||
       app_record->value_type != WmBusApplicationValueUnsigned) {
        return false;
    }

    if(app_record->scale10 >= -3) {
        uint64_t scaled = app_record->value_unsigned;
        if(app_record->scale10 >= 0) {
            scaled *= wmbus_packet_pow10_u64((uint8_t)(app_record->scale10 + 3));
        } else {
            scaled *= wmbus_packet_pow10_u64((uint8_t)(app_record->scale10 + 3));
        }
        if(scaled > UINT32_MAX) return false;
        *total_m3_x1000 = (uint32_t)scaled;
        return true;
    }

    uint8_t divisor_power = (uint8_t)(-3 - app_record->scale10);
    uint64_t divisor = wmbus_packet_pow10_u64(divisor_power);
    if((app_record->value_unsigned % divisor) != 0U) return false;

    uint64_t scaled = app_record->value_unsigned / divisor;
    if(scaled > UINT32_MAX) return false;
    *total_m3_x1000 = (uint32_t)scaled;
    return true;
}

bool wmbus_packet_decode_application_records(
    const uint8_t* payload,
    size_t payload_len,
    WmBusApplicationRecord* out,
    uint8_t out_max,
    uint8_t* out_count) {
    if(out_count) *out_count = 0U;
    if((payload_len > 0U) && (!payload || !out)) return false;
    if(!payload && payload_len == 0U) return true;

    size_t pos = 0U;
    uint8_t count = 0U;

    while(pos < payload_len) {
        while(pos < payload_len && payload[pos] == 0x2FU) {
            pos++;
        }
        if(pos >= payload_len) break;
        if(payload[pos] == 0x0FU || payload[pos] == 0x1FU) {
            break;
        }

        if(count >= out_max) break;

        WmBusApplicationRecord* record = &out[count];
        memset(record, 0, sizeof(*record));

        size_t start = pos;
        record->dif = payload[pos++];

        uint16_t storage_no = (record->dif >> 6) & 0x01U;
        uint8_t storage_shift = 1U;

        for(record->dife_count = 0U; (record->dif & 0x80U) != 0U;) {
            if(pos >= payload_len || record->dife_count >= WMBUS_PACKET_DIFE_MAX) return false;
            uint8_t dife = payload[pos++];
            record->difes[record->dife_count++] = dife;
            storage_no |= (uint16_t)(dife & 0x0FU) << storage_shift;
            storage_shift += 4U;
            record->tariff |= (uint8_t)((dife >> 4) & 0x03U) << (2U * (record->dife_count - 1U));
            record->subunit |= (uint8_t)((dife >> 6) & 0x01U) << (record->dife_count - 1U);
            if((dife & 0x80U) == 0U) break;
        }
        record->storage_no = storage_no;

        if(pos >= payload_len) return false;
        record->vif = payload[pos++];
        for(record->vife_count = 0U; (record->vif & 0x80U) != 0U;) {
            if(pos >= payload_len || record->vife_count >= WMBUS_PACKET_VIFE_MAX) return false;
            uint8_t vife = payload[pos++];
            record->vifes[record->vife_count++] = vife;
            if((vife & 0x80U) == 0U) break;
        }

        bool is_bcd = false;
        bool is_variable_text = false;
        int data_len = wmbus_packet_data_len_from_dif(record->dif, &is_bcd, &is_variable_text);
        if(data_len == -1) return false;
        if(data_len == -2) {
            if(pos >= payload_len) return false;
            data_len = payload[pos++];
        }
        if((size_t)data_len > (payload_len - pos)) return false;
        record->data_len = (uint8_t)data_len;

        wmbus_packet_map_vif(record);
        wmbus_packet_decode_record_value(record, &payload[pos], record->data_len, is_bcd, is_variable_text);

        pos += record->data_len;
        record->record_len = (uint8_t)((pos - start > WMBUS_PACKET_RECORD_RAW_MAX) ?
                                           WMBUS_PACKET_RECORD_RAW_MAX :
                                           (pos - start));
        memcpy(record->raw, &payload[start], record->record_len);
        count++;
    }

    if(out_count) *out_count = count;
    return true;
}

static void wmbus_packet_extract_frame_info(
    const uint8_t* frame,
    size_t frame_len,
    WmBusPacketRecord* record) {
    if(!frame || !record || frame_len < 11U) return;

    record->frame.l_field = frame[0];
    record->frame.c_field = frame[1];
    record->frame.m_field = (uint16_t)frame[2] | ((uint16_t)frame[3] << 8);
    wmbus_frame_decode_mfg(record->frame.m_field, record->frame.mfg);
    memcpy(record->frame.id, &frame[4], sizeof(record->frame.id));
    wmbus_frame_format_id(record->frame.id, record->frame.id_str, &record->frame.id_is_bcd);
    record->frame.version = frame[8];
    record->frame.dev_type = frame[9];
    record->frame.ci_field = frame[10];
    record->transport.header_len = 11U;

    if(frame_len > sizeof(record->frame.normalized)) {
        frame_len = sizeof(record->frame.normalized);
    }
    record->frame.normalized_len = (uint16_t)frame_len;
    memcpy(record->frame.normalized, frame, frame_len);

    if(frame_len >= 15U && wmbus_ci_has_short_tpl(frame[10])) {
        record->transport.has_short_tpl = true;
        record->transport.header_len = 15U;
        record->transport.acc = frame[11];
        record->transport.tpl_status = frame[12];
        record->transport.cfg = (uint16_t)frame[13] | ((uint16_t)frame[14] << 8);
        record->transport.security_mode =
            wmbus_parser_short_tpl_security_mode(record->transport.cfg);
        record->transport.security_likely_encrypted =
            wmbus_parser_short_tpl_security_likely_encrypted(record->transport.cfg);
    }
}

static void
    wmbus_packet_set_raw_payload(WmBusPacketRecord* record, const uint8_t* frame, size_t frame_len) {
    if(!record || !frame) return;

    size_t offset = record->transport.header_len;
    if(frame_len <= offset) {
        record->payload.raw_len = 0U;
        return;
    }

    size_t payload_len = frame_len - offset;
    if(payload_len > sizeof(record->payload.raw_payload)) {
        payload_len = sizeof(record->payload.raw_payload);
    }

    record->payload.raw_len = (uint16_t)payload_len;
    memcpy(record->payload.raw_payload, &frame[offset], payload_len);
}

static void
    wmbus_packet_set_app_payload(WmBusPacketRecord* record, const uint8_t* frame, size_t frame_len) {
    if(!record) return;

    record->payload.has_app_payload = false;
    record->payload.app_len = 0U;
    if(!frame) return;

    size_t offset = record->transport.header_len;
    if(frame_len <= offset) return;

    size_t payload_len = frame_len - offset;
    if(payload_len > sizeof(record->payload.app_payload)) {
        payload_len = sizeof(record->payload.app_payload);
    }

    record->payload.has_app_payload = true;
    record->payload.app_len = (uint16_t)payload_len;
    memcpy(record->payload.app_payload, &frame[offset], payload_len);
}

static void wmbus_packet_add_short_tpl_fields(WmBusPacketRecord* record) {
    if(!record || !record->transport.has_short_tpl) return;

    char temp[WMBUS_PACKET_VALUE_MAX];

    snprintf(temp, sizeof(temp), "%02X", record->transport.acc);
    wmbus_packet_add_field(record, "ACC", temp);

    snprintf(temp, sizeof(temp), "%02X", record->transport.tpl_status);
    wmbus_packet_add_field(record, "TPL", temp);

    snprintf(temp, sizeof(temp), "%04X", record->transport.cfg);
    wmbus_packet_add_field(record, "CFG", temp);

    snprintf(temp, sizeof(temp), "%02X", record->transport.security_mode);
    wmbus_packet_add_field(record, "SEC", temp);

    if(record->transport.decrypted) {
        if(record->transport.key_index != 0U) {
            snprintf(temp, sizeof(temp), "#%u", (unsigned int)record->transport.key_index);
        } else {
            snprintf(temp, sizeof(temp), "zero");
        }
        wmbus_packet_add_field(record, "Key", temp);
    } else if(record->transport.security_likely_encrypted) {
        wmbus_packet_add_field(record, "Payload", "Encrypted");
    }
}

static void wmbus_packet_populate_application_from_records(WmBusPacketRecord* record) {
    if(!record) return;

    for(uint8_t i = 0; i < record->application.record_count; i++) {
        const WmBusApplicationRecord* app_record = &record->application.records[i];
        if(app_record->label[0] != '\0' && app_record->value_text[0] != '\0') {
            wmbus_packet_add_field(record, app_record->label, app_record->value_text);
        }

        if(!record->application.has_total_volume_m3 && app_record->storage_no == 0U) {
            uint32_t total_m3_x1000 = 0U;
            if(wmbus_packet_total_volume_from_record(app_record, &total_m3_x1000)) {
                record->application.has_total_volume_m3 = true;
                record->application.total_volume_m3_x1000 = total_m3_x1000;
            }
        }
    }
}

static bool wmbus_packet_try_parse_current_payload(WmBusPacketRecord* record) {
    if(!record || !record->payload.has_app_payload) return false;

    memset(&record->application, 0, sizeof(record->application));

    uint8_t record_count = 0U;
    bool decode_ok = wmbus_packet_decode_application_records(
        record->payload.app_payload,
        record->payload.app_len,
        record->application.records,
        COUNT_OF(record->application.records),
        &record_count);
    if(decode_ok) {
        record->application.record_count = record_count;
        wmbus_packet_populate_application_from_records(record);
    }

    bool parsed = wmbus_device_parser_apply(record);
    if(!parsed && record->application.record_count > 0U) {
        snprintf(
            record->application.parser_name,
            sizeof(record->application.parser_name),
            "DIF/VIF");
    }

    return parsed || record->application.record_count > 0U;
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
        wmbus_parser_decrypt_mode5(frame, frame_len, record->transport.cfg, key, decrypt_frame);
    if(decrypt.result != WmBusDecryptResultOk) {
        wmbus_packet_log_decrypt_failure(record, key, decrypt.result);
        return false;
    }

    wmbus_packet_set_app_payload(record, decrypt_frame, frame_len);
    bool parsed = wmbus_packet_try_parse_current_payload(record);
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

    record->transport.decrypted = false;
    record->transport.key_index = 0U;
    memset(&record->application, 0, sizeof(record->application));

    wmbus_packet_set_raw_payload(record, frame, frame_len);

    if(!record->transport.has_short_tpl || !record->transport.security_likely_encrypted) {
        wmbus_packet_set_app_payload(record, frame, frame_len);
        return wmbus_packet_try_parse_current_payload(record);
    }

    if(record->transport.security_mode != 0x05U) {
        FURI_LOG_D(
            TAG,
            "decrypt skipped id=%s ci=%02X cfg=%04X unsupported security mode=%02X",
            wmbus_packet_log_id(record),
            record->frame.ci_field,
            record->transport.cfg,
            record->transport.security_mode);
        return false;
    }

    uint8_t decrypt_frame[WMBUS_DECODE_MAX] = {0};

    for(uint8_t i = 0; keyring && i < keyring->count; i++) {
        const WmBusKeyEntry* entry = wmbus_keyring_get(keyring, i);
        if(!entry) continue;

        if(wmbus_packet_try_key(frame, frame_len, entry->key, record, decrypt_frame)) {
            record->transport.decrypted = true;
            record->transport.key_index = i + 1U;
            return true;
        }
    }

    if(wmbus_packet_try_key(frame, frame_len, wmbus_zero_key, record, decrypt_frame)) {
        record->transport.decrypted = true;
        record->transport.key_index = 0U;
        return true;
    }

    memset(&record->application, 0, sizeof(record->application));
    record->payload.has_app_payload = false;
    record->payload.app_len = 0U;
    return false;
}

static void wmbus_packet_finalize_parser(bool parsed, WmBusPacketRecord* record) {
    if(!record) return;

    if(record->application.parser_name[0] == '\0') {
        snprintf(
            record->application.parser_name,
            sizeof(record->application.parser_name),
            "%s",
            record->transport.has_short_tpl ? "Short TPL" :
            (record->packet_is_frame ? "Header" : "Raw"));
    }

    if(!parsed || record->application.field_count == 0U) {
        wmbus_packet_add_short_tpl_fields(record);
    }

    if(record->application.summary_a[0] == '\0') {
        char summary_a[WMBUS_PACKET_VALUE_MAX] = {0};
        char summary_b[WMBUS_PACKET_VALUE_MAX] = {0};

        if(record->transport.has_short_tpl) {
            wmbus_packet_format_security_summary(
                record->transport.has_short_tpl,
                record->transport.security_mode,
                record->transport.security_likely_encrypted,
                record->transport.decrypted,
                record->transport.key_index,
                summary_a,
                sizeof(summary_a));
            if(summary_a[0] == '\0') {
                snprintf(summary_a, sizeof(summary_a), "CI:%02X", record->frame.ci_field);
            }
            snprintf(summary_b, sizeof(summary_b), "CI:%02X", record->frame.ci_field);
        } else if(record->packet_is_frame) {
            snprintf(summary_a, sizeof(summary_a), "CI:%02X", record->frame.ci_field);
            snprintf(summary_b, sizeof(summary_b), "R:%d", record->rssi);
        } else {
            snprintf(summary_a, sizeof(summary_a), "Len:%u bytes", (unsigned int)record->packet_len);
        }

        wmbus_packet_set_summary(record, summary_a, summary_b[0] ? summary_b : NULL);
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

        bool parsed = wmbus_packet_select_parse_frame(frame, frame_len, record, keyring);
        wmbus_packet_finalize_parser(parsed, record);
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

void wmbus_packet_build_fields_text(const WmBusPacketRecord* record, char* out, size_t out_size) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';
    if(!record) return;

    size_t write = 0U;
    for(uint8_t i = 0; i < record->application.field_count; i++) {
        const WmBusPacketField* field = &record->application.fields[i];
        int len = snprintf(
            &out[write],
            out_size - write,
            "%s%s=%s",
            (i == 0U) ? "" : ";",
            field->label,
            field->value);
        if(len < 0) break;
        if((size_t)len >= (out_size - write)) {
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
    bool prefer_parser_primary =
        !wmbus_packet_parser_name_is_generic(record->application.parser_name);

    wmbus_packet_build_fields_text(record, fields, sizeof(fields));
    if(record->application.has_total_volume_m3) {
        wmbus_packet_format_total_m3(record->application.total_volume_m3_x1000, total, sizeof(total));
    }
    wmbus_packet_format_security_text(
        record->transport.has_short_tpl,
        record->transport.security_mode,
        record->transport.security_likely_encrypted,
        record->transport.decrypted,
        record->transport.key_index,
        security,
        sizeof(security));
    if(total[0]) {
        snprintf(summary_a, sizeof(summary_a), "Total: %s\n", total);
    } else if(record->application.summary_a[0] && prefer_parser_primary) {
        snprintf(summary_a, sizeof(summary_a), "%s\n", record->application.summary_a);
    }
    if(security[0]) {
        snprintf(summary_b, sizeof(summary_b), "Security: %s\n", security);
    } else if(record->application.summary_b[0] && prefer_parser_primary) {
        snprintf(summary_b, sizeof(summary_b), "%s\n", record->application.summary_b);
    }
    if(fields[0] && !record->application.has_total_volume_m3 && security[0] == '\0') {
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
        snprintf(parser_line, sizeof(parser_line), "Parser: %s\n", record->application.parser_name);
    }

    if(record->packet_is_frame) {
        snprintf(
            out,
            out_size,
            "Status: %s\nMF:%s  DT:%02X  ID:%s\n%s\nCI:%02X  V:%02X\n%s%s%s%s",
            wmbus_packet_status_str(record->status),
            record->frame.mfg,
            record->frame.dev_type,
            record->frame.id_str,
            mode_line,
            record->frame.ci_field,
            record->frame.version,
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
            record->application.parser_name,
            (unsigned int)record->packet_len);
    }
}
