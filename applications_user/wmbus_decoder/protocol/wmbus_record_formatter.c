#include "wmbus_record_formatter.h"

#include <stdio.h>
#include <string.h>

#include "wmbus_application_record.h"

static uint64_t wmbus_record_formatter_pow10_u64(uint8_t power) {
    uint64_t result = 1U;
    while(power > 0U) {
        result *= 10U;
        power--;
    }
    return result;
}

static void wmbus_record_formatter_scaled_unsigned(
    uint64_t value,
    int8_t scale10,
    char* out,
    size_t out_size) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';

    if(scale10 >= 0) {
        uint64_t scaled = value * wmbus_record_formatter_pow10_u64((uint8_t)scale10);
        snprintf(out, out_size, "%llu", (unsigned long long)scaled);
        return;
    }

    uint8_t decimals = (uint8_t)(-scale10);
    uint64_t div = wmbus_record_formatter_pow10_u64(decimals);
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

static bool wmbus_record_formatter_raw_hex_le(
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

static const char* wmbus_record_formatter_unit(const WmBusApplicationRecord* record) {
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
    wmbus_record_formatter_measurement_type_short(const WmBusApplicationRecord* record) {
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

static uint8_t wmbus_record_formatter_count_quantity(
    const WmBusApplicationRecord* records,
    uint8_t record_count,
    WmBusApplicationQuantity quantity) {
    if(!records || quantity == WmBusApplicationQuantityUnknown) return 0U;

    uint8_t count = 0U;
    for(uint8_t i = 0; i < record_count; i++) {
        if(records[i].quantity == quantity &&
           wmbus_application_record_is_meaningful(&records[i])) {
            count++;
        }
    }

    return count;
}

static bool wmbus_record_formatter_is_primary_summary_candidate(
    const WmBusApplicationRecord* record) {
    if(!record || !wmbus_application_record_is_meaningful(record)) {
        return false;
    }

    return record->storage_no == 0U && record->tariff == 0U && record->subunit == 0U;
}

static bool wmbus_record_formatter_format_label_buf(
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

static bool wmbus_record_formatter_format_value_buf(
    const WmBusApplicationRecord* record,
    char* out,
    size_t out_size) {
    if(!out || out_size == 0U) return false;
    out[0] = '\0';
    if(!record) return false;

    switch(record->value_type) {
    case WmBusApplicationValueUnsigned: {
        wmbus_record_formatter_scaled_unsigned(record->value_unsigned, record->scale10, out, out_size);
        const char* unit = wmbus_record_formatter_unit(record);
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
            return wmbus_record_formatter_raw_hex_le(
                record->value_unsigned, record->data_len, out, out_size);
        }
        return false;
    case WmBusApplicationValueNone:
    default:
        return false;
    }
}

static bool wmbus_record_formatter_format_context_label_buf(
    const WmBusApplicationRecord* record,
    const WmBusApplicationRecord* records,
    uint8_t record_count,
    char* out,
    size_t out_size) {
    if(!wmbus_record_formatter_format_label_buf(record, out, out_size)) {
        return false;
    }

    if(!record || out[0] == '\0') return false;

    uint8_t same_quantity_count =
        wmbus_record_formatter_count_quantity(records, record_count, record->quantity);
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
        const char* mt = wmbus_record_formatter_measurement_type_short(record);
        if(mt) {
            len = snprintf(&out[write], out_size - write, "%s", mt);
            if(len < 0 || (size_t)len >= (out_size - write)) return false;
            write += (size_t)len;
            need_sep = true;
        }
    }

    if(include_storage) {
        len = snprintf(&out[write], out_size - write, "%sS%u", need_sep ? "," : "", record->storage_no);
        if(len < 0 || (size_t)len >= (out_size - write)) return false;
        write += (size_t)len;
        need_sep = true;
    }

    if(include_tariff) {
        len = snprintf(&out[write], out_size - write, "%sT%u", need_sep ? "," : "", record->tariff);
        if(len < 0 || (size_t)len >= (out_size - write)) return false;
        write += (size_t)len;
        need_sep = true;
    }

    if(include_subunit) {
        len = snprintf(&out[write], out_size - write, "%sU%u", need_sep ? "," : "", record->subunit);
        if(len < 0 || (size_t)len >= (out_size - write)) return false;
        write += (size_t)len;
    }

    len = snprintf(&out[write], out_size - write, "]");
    return len >= 0 && (size_t)len < (out_size - write);
}

static bool wmbus_record_formatter_format_field_buf(
    const WmBusApplicationRecord* record,
    const WmBusApplicationRecord* records,
    uint8_t record_count,
    bool include_context,
    char* out,
    size_t out_size) {
    char label[WMBUS_PACKET_LABEL_MAX] = {0};
    char value[WMBUS_PACKET_VALUE_MAX] = {0};
    bool have_label = include_context ?
                          wmbus_record_formatter_format_context_label_buf(
                              record, records, record_count, label, sizeof(label)) :
                          wmbus_record_formatter_format_label_buf(record, label, sizeof(label));
    bool have_value = wmbus_record_formatter_format_value_buf(record, value, sizeof(value));
    if(!have_label || !have_value || label[0] == '\0' || value[0] == '\0') {
        if(out && out_size > 0U) out[0] = '\0';
        return false;
    }

    snprintf(out, out_size, "%s=%s", label, value);
    return true;
}

bool wmbus_record_formatter_format_label(const WmBusApplicationRecord* record, FuriString* out) {
    char label[WMBUS_PACKET_LABEL_MAX] = {0};
    if(!out) return false;

    furi_string_reset(out);
    if(!wmbus_record_formatter_format_label_buf(record, label, sizeof(label)) || label[0] == '\0') {
        return false;
    }

    furi_string_set(out, label);
    return true;
}

bool wmbus_record_formatter_format_value(const WmBusApplicationRecord* record, FuriString* out) {
    char value[WMBUS_PACKET_VALUE_MAX] = {0};
    if(!out) return false;

    furi_string_reset(out);
    if(!wmbus_record_formatter_format_value_buf(record, value, sizeof(value)) || value[0] == '\0') {
        return false;
    }

    furi_string_set(out, value);
    return true;
}

bool wmbus_record_formatter_format_field(const WmBusApplicationRecord* record, FuriString* out) {
    char field[WMBUS_PACKET_DETAIL_MAX] = {0};
    if(!out) return false;

    furi_string_reset(out);
    if(!wmbus_record_formatter_format_field_buf(record, NULL, 0U, false, field, sizeof(field)) ||
       field[0] == '\0') {
        return false;
    }

    furi_string_set(out, field);
    return true;
}

bool wmbus_record_formatter_format_joined(
    const WmBusApplicationRecord* records,
    uint8_t record_count,
    char delimiter,
    FuriString* out) {
    if(!out) return false;
    furi_string_reset(out);
    if(!records || record_count == 0U) return false;

    bool quantity_seen[WmBusApplicationQuantityStatus + 1U] = {0};
    bool wrote_any = false;

    for(uint8_t pass = 0U; pass < 2U; pass++) {
        for(uint8_t i = 0; i < record_count; i++) {
            const WmBusApplicationRecord* record = &records[i];
            char field[WMBUS_PACKET_DETAIL_MAX] = {0};

            if(!wmbus_record_formatter_format_field_buf(
                   record, records, record_count, true, field, sizeof(field))) {
                continue;
            }
            if(record->quantity > WmBusApplicationQuantityStatus ||
               quantity_seen[record->quantity]) {
                continue;
            }
            if(pass == 0U && !wmbus_record_formatter_is_primary_summary_candidate(record)) {
                continue;
            }

            if(wrote_any) {
                furi_string_push_back(out, delimiter);
            }
            furi_string_cat_str(out, field);
            wrote_any = true;
            quantity_seen[record->quantity] = true;

            if(record->quantity != WmBusApplicationQuantityDateTime &&
               record->quantity != WmBusApplicationQuantityStatus &&
               furi_string_size(out) >= (WMBUS_PACKET_DETAIL_MAX / 2U)) {
                return true;
            }
        }
    }

    return wrote_any;
}
