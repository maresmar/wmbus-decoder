#include "wmbus_application_record.h"

#include "parser/wmbus_parser.h"

#include <stdio.h>
#include <string.h>

static uint64_t wmbus_application_pow10_u64(uint8_t power) {
    uint64_t result = 1U;
    while(power > 0U) {
        result *= 10U;
        power--;
    }
    return result;
}

static void wmbus_application_format_scaled_unsigned(
    uint64_t value,
    int8_t scale10,
    char* out,
    size_t out_size) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';

    if(scale10 >= 0) {
        uint64_t scaled = value * wmbus_application_pow10_u64((uint8_t)scale10);
        snprintf(out, out_size, "%llu", (unsigned long long)scaled);
        return;
    }

    uint8_t decimals = (uint8_t)(-scale10);
    uint64_t div = wmbus_application_pow10_u64(decimals);
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

static bool wmbus_application_format_raw_hex_le(
    uint64_t value,
    uint8_t data_len,
    char* out,
    size_t out_size) {
    if(!out || out_size == 0U || data_len == 0U || data_len > 8U) return false;
    out[0] = '\0';

    size_t write = 0U;
    for(uint8_t i = 0; i < data_len; i++) {
        int len = snprintf(
            &out[write], out_size - write, "%02X", (unsigned int)((value >> (8U * i)) & 0xFFU));
        if(len < 0 || (size_t)len >= (out_size - write)) {
            out[0] = '\0';
            return false;
        }
        write += (size_t)len;
    }

    return true;
}

static const char*
    wmbus_application_record_unit(const WmBusApplicationRecord* record) {
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

static const char*
    wmbus_application_record_measurement_type_short(const WmBusApplicationRecord* record) {
    if(!record) return NULL;

    switch(record->measurement_type) {
    case WmBusApplicationMeasurementTypeInstantaneous:
        return "inst";
    case WmBusApplicationMeasurementTypeMinimum:
        return "min";
    case WmBusApplicationMeasurementTypeMaximum:
        return "max";
    case WmBusApplicationMeasurementTypeAtError:
        return "err";
    case WmBusApplicationMeasurementTypeUnknown:
    default:
        return NULL;
    }
}

static uint8_t wmbus_application_count_records_for_quantity(
    const WmBusPacketApplicationData* application,
    WmBusApplicationQuantity quantity) {
    if(!application || quantity == WmBusApplicationQuantityUnknown) return 0U;

    uint8_t count = 0U;
    for(uint8_t i = 0; i < application->record_count; i++) {
        if(application->records[i].quantity == quantity &&
           wmbus_application_record_is_meaningful(&application->records[i])) {
            count++;
        }
    }

    return count;
}

static bool wmbus_application_record_format_label_with_context(
    const WmBusApplicationRecord* record,
    const WmBusPacketApplicationData* application,
    char* out,
    size_t out_size) {
    if(!wmbus_application_record_format_label(record, out, out_size)) {
        return false;
    }

    if(!record || out[0] == '\0') return false;

    uint8_t same_quantity_count =
        wmbus_application_count_records_for_quantity(application, record->quantity);
    bool include_measurement_type =
        same_quantity_count > 1U ||
        (record->measurement_type != WmBusApplicationMeasurementTypeUnknown &&
         record->measurement_type != WmBusApplicationMeasurementTypeInstantaneous);
    bool include_storage = record->storage_no != 0U;
    bool include_tariff = record->tariff != 0U;
    bool include_subunit = record->subunit != 0U;

    if(!(include_measurement_type || include_storage || include_tariff || include_subunit)) {
        return true;
    }

    size_t write = strlen(out);
    int len = snprintf(&out[write], out_size - write, "[");
    if(len < 0 || (size_t)len >= (out_size - write)) return false;
    write += (size_t)len;

    bool need_sep = false;
    if(include_measurement_type) {
        const char* mt = wmbus_application_record_measurement_type_short(record);
        if(mt) {
            len = snprintf(&out[write], out_size - write, "%s", mt);
            if(len < 0 || (size_t)len >= (out_size - write)) return false;
            write += (size_t)len;
            need_sep = true;
        }
    }

    if(include_storage) {
        len = snprintf(
            &out[write], out_size - write, "%sS%u", need_sep ? "," : "", record->storage_no);
        if(len < 0 || (size_t)len >= (out_size - write)) return false;
        write += (size_t)len;
        need_sep = true;
    }

    if(include_tariff) {
        len = snprintf(
            &out[write], out_size - write, "%sT%u", need_sep ? "," : "", record->tariff);
        if(len < 0 || (size_t)len >= (out_size - write)) return false;
        write += (size_t)len;
        need_sep = true;
    }

    if(include_subunit) {
        len = snprintf(
            &out[write], out_size - write, "%sU%u", need_sep ? "," : "", record->subunit);
        if(len < 0 || (size_t)len >= (out_size - write)) return false;
        write += (size_t)len;
    }

    len = snprintf(&out[write], out_size - write, "]");
    return len >= 0 && (size_t)len < (out_size - write);
}

static void wmbus_application_format_short_tpl_fields(
    const WmBusPacketTplData* tpl,
    char* out,
    size_t out_size) {
    if(!tpl || !out || out_size == 0U) return;

    char temp[24];
    size_t write = 0U;

    int len = snprintf(&out[write], out_size - write, "ACC=%02X", tpl->acc);
    if(len < 0 || (size_t)len >= (out_size - write)) return;
    write += (size_t)len;

    len = snprintf(&out[write], out_size - write, ";TPL=%02X", tpl->tpl_status);
    if(len < 0 || (size_t)len >= (out_size - write)) return;
    write += (size_t)len;

    len = snprintf(&out[write], out_size - write, ";CFG=%04X", tpl->cfg);
    if(len < 0 || (size_t)len >= (out_size - write)) return;
    write += (size_t)len;

    len = snprintf(
        &out[write],
        out_size - write,
        ";SEC=%02X",
        tpl->security_mode);
    if(len < 0 || (size_t)len >= (out_size - write)) return;
    write += (size_t)len;

    if(tpl->decrypted) {
        if(tpl->key_index != 0U) {
            snprintf(temp, sizeof(temp), "#%u", (unsigned int)tpl->key_index);
        } else {
            snprintf(temp, sizeof(temp), "zero");
        }
        snprintf(&out[write], out_size - write, ";Key=%s", temp);
    } else if(tpl->has_short_tpl && wmbus_parser_short_tpl_security_likely_encrypted(tpl->cfg)) {
        snprintf(&out[write], out_size - write, ";Payload=Encrypted");
    }
}

void wmbus_application_record_reset(WmBusApplicationRecord* record) {
    if(!record) return;
    memset(record, 0, sizeof(*record));
}

bool wmbus_application_record_append(
    WmBusPacketApplicationData* application,
    WmBusApplicationRecord** out_record) {
    if(!application || !out_record || application->record_count >= COUNT_OF(application->records)) {
        return false;
    }

    WmBusApplicationRecord* record = &application->records[application->record_count++];
    wmbus_application_record_reset(record);
    *out_record = record;
    return true;
}

void wmbus_application_record_set_unsigned(WmBusApplicationRecord* record, uint64_t value) {
    if(!record) return;
    record->value_type = WmBusApplicationValueUnsigned;
    record->value_unsigned = value;
}

bool wmbus_application_record_set_raw_hex_le(
    WmBusApplicationRecord* record,
    const uint8_t* data,
    uint8_t data_len) {
    if(!record || !data || data_len == 0U || data_len > 8U) return false;

    uint64_t value = 0U;
    for(uint8_t i = 0; i < data_len; i++) {
        value |= ((uint64_t)data[i]) << (8U * i);
    }

    record->value_type = WmBusApplicationValueRaw;
    record->value_unsigned = value;
    return true;
}

bool wmbus_application_record_set_date(
    WmBusApplicationRecord* record,
    uint16_t year,
    uint8_t month,
    uint8_t day) {
    if(!record || month == 0U || month > 12U || day == 0U || day > 31U) return false;

    record->value_type = WmBusApplicationValueDateTime;
    record->value_datetime.year = year;
    record->value_datetime.month = month;
    record->value_datetime.day = day;
    record->value_datetime.hour = 0U;
    record->value_datetime.minute = 0U;
    record->value_datetime.has_time = false;
    return true;
}

bool wmbus_application_record_set_datetime(
    WmBusApplicationRecord* record,
    uint16_t year,
    uint8_t month,
    uint8_t day,
    uint8_t hour,
    uint8_t minute) {
    if(!record || month == 0U || month > 12U || day == 0U || day > 31U || hour > 23U ||
       minute > 59U) {
        return false;
    }

    record->value_type = WmBusApplicationValueDateTime;
    record->value_datetime.year = year;
    record->value_datetime.month = month;
    record->value_datetime.day = day;
    record->value_datetime.hour = hour;
    record->value_datetime.minute = minute;
    record->value_datetime.has_time = true;
    return true;
}

bool wmbus_application_record_is_meaningful(const WmBusApplicationRecord* record) {
    return record && record->quantity != WmBusApplicationQuantityUnknown;
}

bool wmbus_application_record_format_label(
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

bool wmbus_application_record_format_value(
    const WmBusApplicationRecord* record,
    char* out,
    size_t out_size) {
    if(!out || out_size == 0U) return false;
    out[0] = '\0';
    if(!record) return false;

    switch(record->value_type) {
    case WmBusApplicationValueUnsigned: {
        wmbus_application_format_scaled_unsigned(
            record->value_unsigned, record->scale10, out, out_size);
        const char* unit = wmbus_application_record_unit(record);
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
        if(record->quantity == WmBusApplicationQuantityStatus) {
            return wmbus_application_format_raw_hex_le(
                record->value_unsigned, record->data_len, out, out_size);
        }
        return false;
    case WmBusApplicationValueNone:
    default:
        return false;
    }
}

bool wmbus_application_record_format_field(
    const WmBusApplicationRecord* record,
    char* label_out,
    size_t label_out_size,
    char* value_out,
    size_t value_out_size) {
    bool have_label =
        wmbus_application_record_format_label_with_context(record, NULL, label_out, label_out_size);
    bool have_value =
        wmbus_application_record_format_value(record, value_out, value_out_size);
    return have_label && have_value && label_out[0] != '\0' && value_out[0] != '\0';
}

static bool wmbus_application_record_is_primary_summary_candidate(
    const WmBusApplicationRecord* record) {
    if(!record || !wmbus_application_record_is_meaningful(record)) {
        return false;
    }

    return record->storage_no == 0U && record->tariff == 0U && record->subunit == 0U;
}

static bool wmbus_application_record_format_field_with_context(
    const WmBusApplicationRecord* record,
    const WmBusPacketApplicationData* application,
    char* label_out,
    size_t label_out_size,
    char* value_out,
    size_t value_out_size) {
    bool have_label = wmbus_application_record_format_label_with_context(
        record, application, label_out, label_out_size);
    bool have_value =
        wmbus_application_record_format_value(record, value_out, value_out_size);
    return have_label && have_value && label_out[0] != '\0' && value_out[0] != '\0';
}

bool wmbus_application_find_total_volume(
    const WmBusPacketApplicationData* application,
    uint32_t* total_m3_x1000) {
    if(!application || !total_m3_x1000) return false;

    for(uint8_t i = 0; i < application->record_count; i++) {
        const WmBusApplicationRecord* record = &application->records[i];
        if(record->storage_no != 0U || record->quantity != WmBusApplicationQuantityVolume ||
           record->value_type != WmBusApplicationValueUnsigned) {
            continue;
        }

        if(record->scale10 >= -3) {
            uint64_t scaled = record->value_unsigned;
            scaled *= wmbus_application_pow10_u64((uint8_t)(record->scale10 + 3));
            if(scaled > UINT32_MAX) continue;
            *total_m3_x1000 = (uint32_t)scaled;
            return true;
        }

        uint8_t divisor_power = (uint8_t)(-3 - record->scale10);
        uint64_t divisor = wmbus_application_pow10_u64(divisor_power);
        if((record->value_unsigned % divisor) != 0U) {
            continue;
        }

        uint64_t scaled = record->value_unsigned / divisor;
        if(scaled > UINT32_MAX) continue;
        *total_m3_x1000 = (uint32_t)scaled;
        return true;
    }

    return false;
}

void wmbus_application_format_fields_text(
    const WmBusPacketApplicationData* application,
    const WmBusPacketTplData* tpl,
    char* out,
    size_t out_size) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';
    if(!application) return;

    size_t write = 0U;
    bool quantity_seen[WmBusApplicationQuantityStatus + 1U] = {0};

    for(uint8_t pass = 0U; pass < 2U; pass++) {
        for(uint8_t i = 0; i < application->record_count; i++) {
            const WmBusApplicationRecord* record = &application->records[i];
            char label[WMBUS_PACKET_LABEL_MAX] = {0};
            char value[WMBUS_PACKET_VALUE_MAX] = {0};

            if(!wmbus_application_record_format_field_with_context(
                   record, application, label, sizeof(label), value, sizeof(value))) {
                continue;
            }

            if(record->quantity > WmBusApplicationQuantityStatus ||
               quantity_seen[record->quantity]) {
                continue;
            }

            if((pass == 0U) &&
               !wmbus_application_record_is_primary_summary_candidate(record)) {
                continue;
            }

            int len = snprintf(
                &out[write],
                out_size - write,
                "%s%s=%s",
                (write == 0U) ? "" : ";",
                label,
                value);
            if(len < 0 || (size_t)len >= (out_size - write)) {
                return;
            }

            quantity_seen[record->quantity] = true;
            write += (size_t)len;

            if(record->quantity == WmBusApplicationQuantityDateTime ||
               record->quantity == WmBusApplicationQuantityStatus) {
                continue;
            }

            if(write >= (out_size / 2U)) {
                return;
            }
        }
    }

    if(write == 0U && tpl && tpl->has_short_tpl) {
        wmbus_application_format_short_tpl_fields(tpl, out, out_size);
    }
}

static void wmbus_application_format_fields_multiline_text(
    const WmBusPacketApplicationData* application,
    const WmBusPacketTplData* tpl,
    char* out,
    size_t out_size) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';
    if(!application) return;

    size_t write = 0U;
    for(uint8_t i = 0; i < application->record_count; i++) {
        char label[WMBUS_PACKET_LABEL_MAX] = {0};
        char value[WMBUS_PACKET_VALUE_MAX] = {0};
        if(!wmbus_application_record_format_field_with_context(
               &application->records[i], application, label, sizeof(label), value, sizeof(value))) {
            continue;
        }

        int len = snprintf(
            &out[write],
            out_size - write,
            "%s%s=%s",
            (write == 0U) ? "" : "\n",
            label,
            value);
        if(len < 0 || (size_t)len >= (out_size - write)) {
            return;
        }
        write += (size_t)len;
    }

    if(write == 0U && tpl && tpl->has_short_tpl) {
        wmbus_application_format_short_tpl_fields(tpl, out, out_size);
    }
}

bool wmbus_packet_parser_name_is_generic(const char* parser_name) {
    if(!parser_name || parser_name[0] == '\0') return true;

    return strcmp(parser_name, "Short TPL") == 0 || strcmp(parser_name, "Header") == 0 ||
           strcmp(parser_name, "Raw") == 0 || strcmp(parser_name, "DIF/VIF") == 0 ||
           strcmp(parser_name, "DifVif") == 0;
}

void wmbus_packet_format_detail_text_sections(
    WmBusStatus status,
    WmBusRxMode mode,
    int rssi,
    bool packet_is_frame,
    uint16_t packet_len,
    const WmBusPacketDllData* dll,
    const WmBusPacketTplData* tpl,
    const WmBusPacketApplicationData* application,
    char* out,
    size_t out_size) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';
    if(!application) return;

    char fields[384] = {0};
    char total[WMBUS_PACKET_VALUE_MAX] = {0};
    char security[48] = {0};
    char detail_fields[416] = {0};
    char parser_line[40] = {0};
    char mode_line[32] = {0};
    const char* detail_tail = "-";
    uint32_t total_m3_x1000 = 0U;

    wmbus_application_format_fields_multiline_text(application, tpl, fields, sizeof(fields));
    if(wmbus_application_find_total_volume(application, &total_m3_x1000)) {
        wmbus_packet_format_total_m3(total_m3_x1000, total, sizeof(total));
    }
    wmbus_packet_format_security_text(
        tpl ? tpl->has_short_tpl : false,
        tpl ? tpl->security_mode : 0U,
        tpl && tpl->has_short_tpl ? wmbus_parser_short_tpl_security_likely_encrypted(tpl->cfg) :
            false,
        tpl ? tpl->decrypted : false,
        tpl ? tpl->key_index : 0U,
        security,
        sizeof(security));

    if(fields[0]) {
        snprintf(detail_fields, sizeof(detail_fields), "Fields:\n%s", fields);
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
        mode == WmBusRxModeT ? 'T' : 'C',
        rssi);
    if(!wmbus_packet_parser_name_is_generic(application->parser_name)) {
        snprintf(parser_line, sizeof(parser_line), "Parser: %s\n", application->parser_name);
    }

    if(packet_is_frame) {
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
            wmbus_packet_status_str(status),
            dll ? dll->mfg : "",
            dll ? dll->dev_type : 0U,
            dll ? dll->id_str : "",
            mode_line,
            dll ? dll->ci_field : 0U,
            dll ? dll->version : 0U,
            parser_line,
            total_line,
            security_line,
            detail_tail);
    } else {
        snprintf(
            out,
            out_size,
            "Status: %s\nMode: %c  RSSI: %d\nParser: %s\nLen: %u bytes",
            wmbus_packet_status_str(status),
            mode == WmBusRxModeT ? 'T' : 'C',
            rssi,
            application->parser_name,
            (unsigned int)packet_len);
    }
}

void wmbus_packet_format_detail_text(const WmBusPacketRecord* record, char* out, size_t out_size) {
    if(!record) {
        if(out && out_size > 0U) out[0] = '\0';
        return;
    }

    wmbus_packet_format_detail_text_sections(
        record->status,
        record->mode,
        record->rssi,
        record->packet_is_frame,
        record->packet_len,
        &record->dll,
        &record->tpl,
        &record->application,
        out,
        out_size);
}
