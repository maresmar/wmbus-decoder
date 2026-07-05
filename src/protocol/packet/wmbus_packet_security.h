#pragma once

#include "wmbus_packet.h"

bool wmbus_packet_resolve_application_payload(
    const uint8_t* frame,
    size_t frame_len,
    WmBusPacketRecord* record,
    const WmBusCryptoKeyStore* key_store);
