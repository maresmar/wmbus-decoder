#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../wmbus_packet.h"

bool wmbus_parser_dif_vif_probe(const WmBusPacketRecord* record);
bool wmbus_parser_dif_vif_parse(WmBusPacketRecord* record);
bool wmbus_packet_decode_application_records(
    const uint8_t* payload,
    size_t payload_len,
    WmBusApplicationRecord* out,
    uint8_t out_max,
    uint8_t* out_count);
uint8_t wmbus_packet_count_meaningful_records(const WmBusPacketRecord* record);
