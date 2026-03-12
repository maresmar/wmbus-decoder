#include "wmbus_parser_dif_vif.h"

#include <stdio.h>
#include <string.h>

#define WMBUS_APATOR162_MFG_OLD 0x8614U

static uint64_t wmbus_packet_pow10_u64(uint8_t power) {
    uint64_t result = 1U;
    while(power > 0U) {
        result *= 10U;
        power--;
    }
    return result;
}

static void wmbus_packet_format_scaled_unsigned(
    uint64_t value,
    int8_t scale10,
    char* out,
    size_t out_size) {
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

static bool
    wmbus_packet_decode_unsigned_le(const uint8_t* data, uint8_t data_len, uint64_t* value) {
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

static bool wmbus_packet_decode_date2(const uint8_t* data, char* out, size_t out_size) {
    if(!data || !out || out_size == 0U) return false;

    uint8_t day = data[0] & 0x1FU;
    uint8_t month = data[1] & 0x0FU;
    uint16_t year = (uint16_t)(((data[0] >> 5) & 0x07U) | ((data[1] >> 1) & 0x78U)) + 2000U;
    if(day == 0U || day > 31U || month == 0U || month > 12U) return false;

    snprintf(
        out,
        out_size,
        "%04u-%02u-%02u",
        (unsigned int)year,
        (unsigned int)month,
        (unsigned int)day);
    return true;
}

static bool wmbus_packet_decode_datetime4(const uint8_t* data, char* out, size_t out_size) {
    if(!data || !out || out_size == 0U) return false;

    uint8_t minute = data[0] & 0x3FU;
    uint8_t hour = data[1] & 0x1FU;
    uint8_t day = data[2] & 0x1FU;
    uint8_t month = data[3] & 0x0FU;
    uint16_t year = (uint16_t)(((data[2] >> 5) & 0x07U) | ((data[3] >> 1) & 0x78U)) + 2000U;

    if(minute > 59U || hour > 23U || day == 0U || day > 31U || month == 0U || month > 12U) {
        return false;
    }

    snprintf(
        out,
        out_size,
        "%04u-%02u-%02u %02u:%02u",
        (unsigned int)year,
        (unsigned int)month,
        (unsigned int)day,
        (unsigned int)hour,
        (unsigned int)minute);
    return true;
}

static void wmbus_packet_format_hex_text(
    const uint8_t* data,
    uint8_t data_len,
    char* out,
    size_t out_size) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';
    if(!data) return;

    size_t write = 0U;
    for(uint8_t i = 0; i < data_len && (write + 2U) < out_size; i++) {
        snprintf(&out[write], out_size - write, "%02X", data[i]);
        write += 2U;
    }
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
    } else if((record->vif & 0xF8U) == 0x28U) {
        record->quantity = WmBusApplicationQuantityPower;
        record->scale10 = (int8_t)(record->vif & 0x07U) - 3;
        snprintf(record->label, sizeof(record->label), "Power");
        snprintf(record->unit, sizeof(record->unit), "W");
    } else if((record->vif & 0xF8U) == 0x38U) {
        record->quantity = WmBusApplicationQuantityVolumeFlow;
        record->scale10 = (int8_t)(record->vif & 0x07U) - 6;
        snprintf(record->label, sizeof(record->label), "Flow");
        snprintf(record->unit, sizeof(record->unit), "m3/h");
    } else if((record->vif & 0xFCU) == 0x58U) {
        record->quantity = WmBusApplicationQuantityFlowTemperature;
        record->scale10 = (int8_t)(record->vif & 0x03U) - 3;
        snprintf(record->label, sizeof(record->label), "Flow temp");
        snprintf(record->unit, sizeof(record->unit), "C");
    } else if((record->vif & 0xFCU) == 0x5CU) {
        record->quantity = WmBusApplicationQuantityReturnTemperature;
        record->scale10 = (int8_t)(record->vif & 0x03U) - 3;
        snprintf(record->label, sizeof(record->label), "Return temp");
        snprintf(record->unit, sizeof(record->unit), "C");
    } else if((record->vif & 0xFCU) == 0x60U) {
        record->quantity = WmBusApplicationQuantityTemperatureDifference;
        record->scale10 = (int8_t)(record->vif & 0x03U) - 3;
        snprintf(record->label, sizeof(record->label), "Delta temp");
        snprintf(record->unit, sizeof(record->unit), "K");
    } else if(record->vif == 0x6CU) {
        record->quantity = WmBusApplicationQuantityDate;
        snprintf(record->label, sizeof(record->label), "Date");
    } else if(record->vif == 0x6DU) {
        record->quantity = WmBusApplicationQuantityDateTime;
        snprintf(record->label, sizeof(record->label), "Measured at");
    } else if(record->vif == 0xFDU && record->vife_count > 0U && record->vifes[0] == 0x17U) {
        record->quantity = WmBusApplicationQuantityStatus;
        snprintf(record->label, sizeof(record->label), "Status");
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

    if(record->quantity == WmBusApplicationQuantityDate && data_len == 2U) {
        record->value_type = WmBusApplicationValueText;
        if(wmbus_packet_decode_date2(data, record->value_text, sizeof(record->value_text))) {
            return;
        }
    } else if(record->quantity == WmBusApplicationQuantityDateTime && data_len == 4U) {
        record->value_type = WmBusApplicationValueText;
        if(wmbus_packet_decode_datetime4(data, record->value_text, sizeof(record->value_text))) {
            return;
        }
    } else if(record->quantity == WmBusApplicationQuantityStatus) {
        record->value_type = WmBusApplicationValueText;
        wmbus_packet_format_hex_text(
            data, data_len, record->value_text, sizeof(record->value_text));
        return;
    }

    if(is_variable_text) {
        record->value_type = WmBusApplicationValueText;
        wmbus_packet_format_hex_text(
            data, data_len, record->value_text, sizeof(record->value_text));
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
        wmbus_packet_format_hex_text(
            data, data_len, record->value_text, sizeof(record->value_text));
        return;
    }

    record->value_type = WmBusApplicationValueUnsigned;
    record->value_unsigned = value;
    wmbus_packet_format_scaled_unsigned(
        value, record->scale10, record->value_text, sizeof(record->value_text));
    if(record->unit[0] != '\0') {
        size_t len = strlen(record->value_text);
        snprintf(&record->value_text[len], sizeof(record->value_text) - len, " %s", record->unit);
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
        scaled *= wmbus_packet_pow10_u64((uint8_t)(app_record->scale10 + 3));
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
        wmbus_packet_decode_record_value(
            record, &payload[pos], record->data_len, is_bcd, is_variable_text);

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

void wmbus_packet_populate_application_from_records(WmBusPacketRecord* record) {
    if(!record) return;

    for(uint8_t i = 0; i < record->application.record_count; i++) {
        const WmBusApplicationRecord* app_record = &record->application.records[i];
        if(app_record->label[0] != '\0' && app_record->value_text[0] != '\0') {
            if(record->application.field_count >= WMBUS_PACKET_FIELD_MAX) break;

            WmBusPacketField* field =
                &record->application.fields[record->application.field_count++];
            snprintf(field->label, sizeof(field->label), "%s", app_record->label);
            snprintf(field->value, sizeof(field->value), "%s", app_record->value_text);
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

uint8_t wmbus_packet_count_meaningful_records(const WmBusPacketRecord* record) {
    if(!record) return 0U;

    uint8_t count = 0U;
    for(uint8_t i = 0; i < record->application.record_count; i++) {
        const WmBusApplicationRecord* app_record = &record->application.records[i];
        if(app_record->quantity != WmBusApplicationQuantityUnknown) {
            count++;
        }
    }

    return count;
}

bool wmbus_parser_dif_vif_probe(const WmBusPacketRecord* record) {
    return record && record->payload.has_app_payload && record->payload.app_len > 0U;
}

bool wmbus_parser_dif_vif_parse(WmBusPacketRecord* record) {
    if(!wmbus_parser_dif_vif_probe(record)) {
        return false;
    }

    uint8_t record_count = 0U;
    if(!wmbus_packet_decode_application_records(
           record->payload.app_payload,
           record->payload.app_len,
           record->application.records,
           (uint8_t)(sizeof(record->application.records) / sizeof(record->application.records[0])),
           &record_count)) {
        return false;
    }

    record->application.record_count = record_count;
    wmbus_packet_populate_application_from_records(record);

    return record_count > 0U;
}
