#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../protocol/wmbus_packet.h"

bool wmbus_format_record_label(
    const WmBusApplicationRecord* record,
    char* out,
    size_t out_size);
bool wmbus_format_record_value(
    const WmBusApplicationRecord* record,
    char* out,
    size_t out_size);
bool wmbus_format_record_field(
    const WmBusApplicationRecord* record,
    char* label_out,
    size_t label_out_size,
    char* value_out,
    size_t value_out_size);
bool wmbus_format_find_total_volume(
    const WmBusPacketRecord* record,
    uint32_t* total_m3_x1000);
void wmbus_format_fields_text(const WmBusPacketRecord* record, char* out, size_t out_size);
void wmbus_format_detail_text(const WmBusPacketRecord* record, char* out, size_t out_size);
