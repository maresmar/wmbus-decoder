#include "wmbus_application_record.h"

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

void wmbus_application_format_volume_m3(
    uint32_t total_m3_x1000,
    char* out,
    size_t out_size,
    bool with_unit) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';

    uint32_t whole = total_m3_x1000 / 1000U;
    uint32_t frac = total_m3_x1000 % 1000U;
    snprintf(
        out,
        out_size,
        with_unit ? "%lu.%03lu m3" : "%lu.%03lu",
        (unsigned long)whole,
        (unsigned long)frac);
}
