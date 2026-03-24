#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "wmbus_packet.h"

void wmbus_application_record_reset(WmBusApplicationRecord* record);
bool wmbus_application_record_append(
    WmBusPacketApplicationData* application,
    WmBusApplicationRecord** out_record);
void wmbus_application_record_set_unsigned(WmBusApplicationRecord* record, uint64_t value);
bool wmbus_application_record_set_raw_hex_le(
    WmBusApplicationRecord* record,
    const uint8_t* data,
    uint8_t data_len);
bool wmbus_application_record_set_date(
    WmBusApplicationRecord* record,
    uint16_t year,
    uint8_t month,
    uint8_t day);
bool wmbus_application_record_set_datetime(
    WmBusApplicationRecord* record,
    uint16_t year,
    uint8_t month,
    uint8_t day,
    uint8_t hour,
    uint8_t minute);

bool wmbus_application_record_is_meaningful(const WmBusApplicationRecord* record);
bool wmbus_application_find_total_volume(
    const WmBusApplicationRecord* records,
    uint8_t record_count,
    uint32_t* total_m3_x1000);
