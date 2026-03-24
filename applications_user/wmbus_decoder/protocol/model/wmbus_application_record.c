#include "wmbus_application_record.h"

#include <string.h>

static uint64_t wmbus_application_pow10_u64(uint8_t power) {
    uint64_t result = 1U;
    while(power > 0U) {
        result *= 10U;
        power--;
    }
    return result;
}

void wmbus_application_record_reset(WmBusApplicationRecord* record) {
    if(!record) return;
    memset(record, 0, sizeof(*record));
}

bool wmbus_application_record_append(
    WmBusPacketApplicationData* application,
    WmBusApplicationRecord** out_record) {
    if(!application || !out_record || application->record_count >= WMBUS_PACKET_RECORD_MAX) {
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

bool wmbus_application_find_total_volume(
    const WmBusApplicationRecord* records,
    uint8_t record_count,
    uint32_t* total_m3_x1000) {
    if(!records || !total_m3_x1000) return false;

    for(uint8_t i = 0; i < record_count; i++) {
        const WmBusApplicationRecord* record = &records[i];
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
