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
bool wmbus_application_record_format_label(
    const WmBusApplicationRecord* record,
    char* out,
    size_t out_size);
bool wmbus_application_record_format_value(
    const WmBusApplicationRecord* record,
    char* out,
    size_t out_size);
bool wmbus_application_record_format_field(
    const WmBusApplicationRecord* record,
    char* label_out,
    size_t label_out_size,
    char* value_out,
    size_t value_out_size);
bool wmbus_application_find_total_volume(
    const WmBusPacketApplicationData* application,
    uint32_t* total_m3_x1000);
void wmbus_application_format_fields_text(
    const WmBusPacketApplicationData* application,
    const WmBusPacketTplData* tpl,
    char* out,
    size_t out_size);
bool wmbus_packet_parser_name_is_generic(const char* parser_name);
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
    size_t out_size);
void wmbus_packet_format_detail_text(const WmBusPacketRecord* record, char* out, size_t out_size);
