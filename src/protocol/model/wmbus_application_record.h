#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "wmbus_application_types.h"

void wmbus_application_record_reset(WmBusApplicationRecord* record);
bool wmbus_application_record_append(
    WmBusPacketApplicationData* application,
    WmBusApplicationRecord** out_record);
void wmbus_application_record_set_unsigned(WmBusApplicationRecord* record, uint64_t value);
bool wmbus_application_record_set_raw_hex_le(
    WmBusApplicationRecord* record,
    const uint8_t* data,
    uint8_t data_len);

bool wmbus_application_record_is_meaningful(const WmBusApplicationRecord* record);
bool wmbus_application_find_total_volume(
    const WmBusApplicationRecord* records,
    uint8_t record_count,
    uint32_t* total_m3_x1000);
bool wmbus_application_format_total_volume_m3(
    const WmBusPacketApplicationData* application,
    char* out,
    size_t out_size,
    bool with_unit);
void wmbus_application_format_scaled_unsigned(
    uint64_t value,
    int8_t scale10,
    char* out,
    size_t out_size);
