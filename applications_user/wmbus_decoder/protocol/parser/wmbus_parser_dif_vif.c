#include "wmbus_parser_dif_vif.h"

#include "../wmbus_application_record.h"

#include <string.h>

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

static bool wmbus_packet_decode_date2(const uint8_t* data, WmBusApplicationDateTime* out) {
    if(!data || !out) return false;

    uint8_t day = data[0] & 0x1FU;
    uint8_t month = data[1] & 0x0FU;
    uint16_t year = (uint16_t)(((data[0] >> 5) & 0x07U) | ((data[1] >> 1) & 0x78U)) + 2000U;
    if(day == 0U || day > 31U || month == 0U || month > 12U) return false;

    out->year = year;
    out->month = month;
    out->day = day;
    out->hour = 0U;
    out->minute = 0U;
    out->has_time = false;
    return true;
}

static bool wmbus_packet_decode_datetime4(const uint8_t* data, WmBusApplicationDateTime* out) {
    if(!data || !out) return false;

    uint8_t minute = data[0] & 0x3FU;
    uint8_t hour = data[1] & 0x1FU;
    uint8_t day = data[2] & 0x1FU;
    uint8_t month = data[3] & 0x0FU;
    uint16_t year = (uint16_t)(((data[2] >> 5) & 0x07U) | ((data[3] >> 1) & 0x78U)) + 2000U;

    if(minute > 59U || hour > 23U || day == 0U || day > 31U || month == 0U || month > 12U) {
        return false;
    }

    out->year = year;
    out->month = month;
    out->day = day;
    out->hour = hour;
    out->minute = minute;
    out->has_time = true;
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

static WmBusApplicationMeasurementType
    wmbus_packet_measurement_type_from_dif(uint8_t dif) {
    switch(dif & 0x30U) {
    case 0x00U:
        return WmBusApplicationMeasurementTypeInstantaneous;
    case 0x10U:
        return WmBusApplicationMeasurementTypeMaximum;
    case 0x20U:
        return WmBusApplicationMeasurementTypeMinimum;
    case 0x30U:
        return WmBusApplicationMeasurementTypeAtError;
    default:
        return WmBusApplicationMeasurementTypeUnknown;
    }
}

static void wmbus_packet_map_vif(
    WmBusApplicationRecord* record,
    uint8_t first_vife,
    bool has_first_vife) {
    if(!record) return;

    record->quantity = WmBusApplicationQuantityUnknown;
    record->scale10 = 0;

    if((record->vif & 0xF8U) == 0x10U) {
        record->quantity = WmBusApplicationQuantityVolume;
        record->scale10 = (int8_t)(record->vif & 0x07U) - 6;
    } else if((record->vif & 0xF8U) == 0x00U) {
        record->quantity = WmBusApplicationQuantityEnergy;
        record->scale10 = (int8_t)(record->vif & 0x07U) - 3;
    } else if((record->vif & 0xF8U) == 0x28U) {
        record->quantity = WmBusApplicationQuantityPower;
        record->scale10 = (int8_t)(record->vif & 0x07U) - 3;
    } else if((record->vif & 0xF8U) == 0x38U) {
        record->quantity = WmBusApplicationQuantityVolumeFlow;
        record->scale10 = (int8_t)(record->vif & 0x07U) - 6;
    } else if((record->vif & 0xFCU) == 0x58U) {
        record->quantity = WmBusApplicationQuantityFlowTemperature;
        record->scale10 = (int8_t)(record->vif & 0x03U) - 3;
    } else if((record->vif & 0xFCU) == 0x5CU) {
        record->quantity = WmBusApplicationQuantityReturnTemperature;
        record->scale10 = (int8_t)(record->vif & 0x03U) - 3;
    } else if((record->vif & 0xFCU) == 0x60U) {
        record->quantity = WmBusApplicationQuantityTemperatureDifference;
        record->scale10 = (int8_t)(record->vif & 0x03U) - 3;
    } else if(record->vif == 0x6CU) {
        record->quantity = WmBusApplicationQuantityDate;
    } else if(record->vif == 0x6DU) {
        record->quantity = WmBusApplicationQuantityDateTime;
    } else if(record->vif == 0xFDU && has_first_vife && first_vife == 0x17U) {
        record->quantity = WmBusApplicationQuantityStatus;
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
    record->value_unsigned = 0U;
    memset(&record->value_datetime, 0, sizeof(record->value_datetime));

    if(data_len == 0U) {
        return;
    }

    if(record->quantity == WmBusApplicationQuantityDate && data_len == 2U) {
        if(wmbus_packet_decode_date2(data, &record->value_datetime)) {
            record->value_type = WmBusApplicationValueDateTime;
            return;
        }
    } else if(record->quantity == WmBusApplicationQuantityDateTime && data_len == 4U) {
        if(wmbus_packet_decode_datetime4(data, &record->value_datetime)) {
            record->value_type = WmBusApplicationValueDateTime;
            return;
        }
    }

    if(record->quantity == WmBusApplicationQuantityStatus) {
        if(!wmbus_application_record_set_raw_hex_le(record, data, data_len)) {
            record->value_type = WmBusApplicationValueRaw;
        }
        return;
    }

    if(is_variable_text) {
        record->value_type = WmBusApplicationValueRaw;
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
        record->value_type = WmBusApplicationValueRaw;
        return;
    }

    wmbus_application_record_set_unsigned(record, value);
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
        wmbus_application_record_reset(record);

        record->dif = payload[pos++];
        record->measurement_type = wmbus_packet_measurement_type_from_dif(record->dif);

        uint16_t storage_no = (record->dif >> 6) & 0x01U;
        uint8_t storage_shift = 1U;
        uint8_t dife_index = 0U;

        while((record->dif & 0x80U) != 0U) {
            if(pos >= payload_len) return false;
            uint8_t dife = payload[pos++];
            storage_no |= (uint16_t)(dife & 0x0FU) << storage_shift;
            storage_shift += 4U;
            record->tariff |= (uint8_t)((dife >> 4) & 0x03U) << (2U * dife_index);
            record->subunit |= (uint8_t)((dife >> 6) & 0x01U) << dife_index;
            dife_index++;
            if((dife & 0x80U) == 0U) break;
        }
        record->storage_no = storage_no;

        if(pos >= payload_len) return false;
        record->vif = payload[pos++];

        uint8_t first_vife = 0U;
        bool has_first_vife = false;
        while((record->vif & 0x80U) != 0U) {
            if(pos >= payload_len) return false;
            uint8_t vife = payload[pos++];
            if(!has_first_vife) {
                first_vife = vife;
                has_first_vife = true;
            }
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

        wmbus_packet_map_vif(record, first_vife, has_first_vife);
        wmbus_packet_decode_record_value(
            record, &payload[pos], record->data_len, is_bcd, is_variable_text);
        pos += record->data_len;
        count++;
    }

    if(out_count) *out_count = count;
    return true;
}

uint8_t wmbus_packet_count_meaningful_records(const WmBusPacketRecord* record) {
    if(!record) return 0U;

    uint8_t count = 0U;
    for(uint8_t i = 0; i < record->application.record_count; i++) {
        const WmBusApplicationRecord* app_record = &record->application.records[i];
        if(wmbus_application_record_is_meaningful(app_record)) {
            count++;
        }
    }

    return count;
}

bool wmbus_parser_dif_vif_probe(
    const WmBusPacketRecord* record,
    const WmBusPacketParseContext* parse_context) {
    return record && parse_context && parse_context->has_application_payload &&
           parse_context->application_len > 0U;
}

bool wmbus_parser_dif_vif_parse(
    WmBusPacketRecord* record,
    const WmBusPacketParseContext* parse_context) {
    if(!wmbus_parser_dif_vif_probe(record, parse_context)) {
        return false;
    }

    uint8_t record_count = 0U;
    if(!wmbus_packet_decode_application_records(
           parse_context->application_payload,
           parse_context->application_len,
           record->application.records,
           (uint8_t)(sizeof(record->application.records) / sizeof(record->application.records[0])),
           &record_count)) {
        return false;
    }

    record->application.record_count = record_count;
    return wmbus_packet_count_meaningful_records(record) > 0U;
}
