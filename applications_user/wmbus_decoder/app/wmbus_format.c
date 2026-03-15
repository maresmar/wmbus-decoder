#include "wmbus_format.h"

#include <stdio.h>
#include <string.h>

static uint64_t wmbus_format_pow10_u64(uint8_t power) {
    uint64_t result = 1U;
    while(power > 0U) {
        result *= 10U;
        power--;
    }
    return result;
}

static void wmbus_format_hex(const uint8_t* data, size_t data_len, char* out, size_t out_size) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';
    if(!data) return;

    size_t write = 0U;
    for(size_t i = 0; i < data_len && (write + 2U) < out_size; i++) {
        snprintf(&out[write], out_size - write, "%02X", data[i]);
        write += 2U;
    }
}

static void wmbus_format_scaled_unsigned(
    uint64_t value,
    int8_t scale10,
    char* out,
    size_t out_size) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';

    if(scale10 >= 0) {
        uint64_t scaled = value * wmbus_format_pow10_u64((uint8_t)scale10);
        snprintf(out, out_size, "%llu", (unsigned long long)scaled);
        return;
    }

    uint8_t decimals = (uint8_t)(-scale10);
    uint64_t div = wmbus_format_pow10_u64(decimals);
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

static const char* wmbus_format_record_unit(const WmBusApplicationRecord* record) {
    if(!record) return NULL;

    switch(record->quantity) {
    case WmBusApplicationQuantityVolume:
        return "m3";
    case WmBusApplicationQuantityEnergy:
        return "Wh";
    case WmBusApplicationQuantityPower:
        return "W";
    case WmBusApplicationQuantityVolumeFlow:
        return "m3/h";
    case WmBusApplicationQuantityFlowTemperature:
    case WmBusApplicationQuantityReturnTemperature:
        return "C";
    case WmBusApplicationQuantityTemperatureDifference:
        return "K";
    default:
        return NULL;
    }
}

bool wmbus_format_record_label(
    const WmBusApplicationRecord* record,
    char* out,
    size_t out_size) {
    if(!out || out_size == 0U) return false;
    out[0] = '\0';
    if(!record) return false;

    const char* label = "Data";
    switch(record->quantity) {
    case WmBusApplicationQuantityVolume:
        label = "Volume";
        break;
    case WmBusApplicationQuantityEnergy:
        label = "Energy";
        break;
    case WmBusApplicationQuantityPower:
        label = "Power";
        break;
    case WmBusApplicationQuantityVolumeFlow:
        label = "Flow";
        break;
    case WmBusApplicationQuantityFlowTemperature:
        label = "Flow temp";
        break;
    case WmBusApplicationQuantityReturnTemperature:
        label = "Return temp";
        break;
    case WmBusApplicationQuantityTemperatureDifference:
        label = "Delta temp";
        break;
    case WmBusApplicationQuantityDate:
        label = "Date";
        break;
    case WmBusApplicationQuantityDateTime:
        label = "Measured at";
        break;
    case WmBusApplicationQuantityStatus:
        label = "Status";
        break;
    case WmBusApplicationQuantityUnknown:
    default:
        break;
    }

    snprintf(out, out_size, "%s", label);
    return true;
}

bool wmbus_format_record_value(
    const WmBusApplicationRecord* record,
    char* out,
    size_t out_size) {
    if(!out || out_size == 0U) return false;
    out[0] = '\0';
    if(!record) return false;

    switch(record->value_type) {
    case WmBusApplicationValueUnsigned: {
        wmbus_format_scaled_unsigned(record->value_unsigned, record->scale10, out, out_size);
        const char* unit = wmbus_format_record_unit(record);
        if(unit && out[0] != '\0') {
            size_t len = strlen(out);
            snprintf(&out[len], out_size - len, " %s", unit);
        }
        return true;
    }
    case WmBusApplicationValueDateTime:
        if(record->value_datetime.has_time) {
            snprintf(
                out,
                out_size,
                "%04u-%02u-%02u %02u:%02u",
                (unsigned int)record->value_datetime.year,
                (unsigned int)record->value_datetime.month,
                (unsigned int)record->value_datetime.day,
                (unsigned int)record->value_datetime.hour,
                (unsigned int)record->value_datetime.minute);
        } else {
            snprintf(
                out,
                out_size,
                "%04u-%02u-%02u",
                (unsigned int)record->value_datetime.year,
                (unsigned int)record->value_datetime.month,
                (unsigned int)record->value_datetime.day);
        }
        return true;
    case WmBusApplicationValueRaw:
        if(record->data_len == 0U && record->raw_len == 0U) return false;
        if(record->raw_len >= record->data_len) {
            wmbus_format_hex(
                &record->raw[record->raw_len - record->data_len], record->data_len, out, out_size);
        } else {
            wmbus_format_hex(record->raw, record->raw_len, out, out_size);
        }
        return true;
    case WmBusApplicationValueNone:
    default:
        return false;
    }
}

bool wmbus_format_record_field(
    const WmBusApplicationRecord* record,
    char* label_out,
    size_t label_out_size,
    char* value_out,
    size_t value_out_size) {
    bool have_label = wmbus_format_record_label(record, label_out, label_out_size);
    bool have_value = wmbus_format_record_value(record, value_out, value_out_size);
    return have_label && have_value && label_out[0] != '\0' && value_out[0] != '\0';
}

bool wmbus_format_find_total_volume(
    const WmBusPacketRecord* record,
    uint32_t* total_m3_x1000) {
    if(!record || !total_m3_x1000) return false;

    for(uint8_t i = 0; i < record->application.record_count; i++) {
        const WmBusApplicationRecord* app_record = &record->application.records[i];
        if(app_record->storage_no != 0U ||
           app_record->quantity != WmBusApplicationQuantityVolume ||
           app_record->value_type != WmBusApplicationValueUnsigned) {
            continue;
        }

        if(app_record->scale10 >= -3) {
            uint64_t scaled = app_record->value_unsigned;
            scaled *= wmbus_format_pow10_u64((uint8_t)(app_record->scale10 + 3));
            if(scaled > UINT32_MAX) continue;
            *total_m3_x1000 = (uint32_t)scaled;
            return true;
        }

        uint8_t divisor_power = (uint8_t)(-3 - app_record->scale10);
        uint64_t divisor = wmbus_format_pow10_u64(divisor_power);
        if((app_record->value_unsigned % divisor) != 0U) {
            continue;
        }

        uint64_t scaled = app_record->value_unsigned / divisor;
        if(scaled > UINT32_MAX) continue;
        *total_m3_x1000 = (uint32_t)scaled;
        return true;
    }

    return false;
}

static void wmbus_format_append_short_tpl_fields(
    const WmBusPacketRecord* record,
    char* out,
    size_t out_size) {
    if(!record || !out || out_size == 0U) return;

    char temp[24];
    size_t write = 0U;

    snprintf(temp, sizeof(temp), "%02X", record->transport.acc);
    int len = snprintf(&out[write], out_size - write, "ACC=%s", temp);
    if(len < 0 || (size_t)len >= (out_size - write)) return;
    write += (size_t)len;

    snprintf(temp, sizeof(temp), "%02X", record->transport.tpl_status);
    len = snprintf(&out[write], out_size - write, ";TPL=%s", temp);
    if(len < 0 || (size_t)len >= (out_size - write)) return;
    write += (size_t)len;

    snprintf(temp, sizeof(temp), "%04X", record->transport.cfg);
    len = snprintf(&out[write], out_size - write, ";CFG=%s", temp);
    if(len < 0 || (size_t)len >= (out_size - write)) return;
    write += (size_t)len;

    snprintf(temp, sizeof(temp), "%02X", record->transport.security_mode);
    len = snprintf(&out[write], out_size - write, ";SEC=%s", temp);
    if(len < 0 || (size_t)len >= (out_size - write)) return;
    write += (size_t)len;

    if(record->transport.decrypted) {
        if(record->transport.key_index != 0U) {
            snprintf(temp, sizeof(temp), "#%u", (unsigned int)record->transport.key_index);
        } else {
            snprintf(temp, sizeof(temp), "zero");
        }
        snprintf(&out[write], out_size - write, ";Key=%s", temp);
    } else if(record->transport.security_likely_encrypted) {
        snprintf(&out[write], out_size - write, ";Payload=Encrypted");
    }
}

static bool wmbus_format_parser_name_is_generic(const char* parser_name) {
    if(!parser_name || parser_name[0] == '\0') return true;

    return strcmp(parser_name, "Short TPL") == 0 || strcmp(parser_name, "Header") == 0 ||
           strcmp(parser_name, "Raw") == 0 || strcmp(parser_name, "DIF/VIF") == 0 ||
           strcmp(parser_name, "DifVif") == 0;
}

void wmbus_format_fields_text(const WmBusPacketRecord* record, char* out, size_t out_size) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';
    if(!record) return;

    size_t write = 0U;
    for(uint8_t i = 0; i < record->application.record_count; i++) {
        char label[WMBUS_PACKET_LABEL_MAX] = {0};
        char value[WMBUS_PACKET_VALUE_MAX] = {0};
        if(!wmbus_format_record_field(
               &record->application.records[i], label, sizeof(label), value, sizeof(value))) {
            continue;
        }

        int len = snprintf(
            &out[write], out_size - write, "%s%s=%s", (write == 0U) ? "" : ";", label, value);
        if(len < 0 || (size_t)len >= (out_size - write)) {
            return;
        }
        write += (size_t)len;
    }

    if(write == 0U && record->transport.has_short_tpl) {
        wmbus_format_append_short_tpl_fields(record, out, out_size);
    }
}

void wmbus_format_detail_text(const WmBusPacketRecord* record, char* out, size_t out_size) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';
    if(!record) return;

    char fields[384] = {0};
    char total[WMBUS_PACKET_VALUE_MAX] = {0};
    char security[48] = {0};
    char detail_fields[416] = {0};
    char parser_line[40] = {0};
    char mode_line[32] = {0};
    const char* detail_tail = "-";
    uint32_t total_m3_x1000 = 0U;

    wmbus_format_fields_text(record, fields, sizeof(fields));
    if(wmbus_format_find_total_volume(record, &total_m3_x1000)) {
        wmbus_packet_format_total_m3(total_m3_x1000, total, sizeof(total));
    }
    wmbus_packet_format_security_text(
        record->transport.has_short_tpl,
        record->transport.security_mode,
        record->transport.security_likely_encrypted,
        record->transport.decrypted,
        record->transport.key_index,
        security,
        sizeof(security));
    if(fields[0]) {
        snprintf(detail_fields, sizeof(detail_fields), "Fields: %s", fields);
    }
    if(detail_fields[0]) {
        detail_tail = detail_fields;
    } else if(total[0] || security[0]) {
        detail_tail = "";
    }

    snprintf(
        mode_line,
        sizeof(mode_line),
        "M:%c  R:%d",
        record->mode == WmBusRxModeT ? 'T' : 'C',
        record->rssi);
    if(!wmbus_format_parser_name_is_generic(record->application.parser_name)) {
        snprintf(parser_line, sizeof(parser_line), "Parser: %s\n", record->application.parser_name);
    }

    if(record->packet_is_frame) {
        char total_line[48] = {0};
        char security_line[64] = {0};
        if(total[0]) {
            snprintf(total_line, sizeof(total_line), "Total: %s\n", total);
        }
        if(security[0]) {
            snprintf(security_line, sizeof(security_line), "Security: %s\n", security);
        }
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
            total_line,
            security_line,
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
