#pragma once

#include "../packet/wmbus_packet.h"

typedef struct {
    const WmBusPacketDllData* dll;
    const WmBusPacketTplData* tpl;
    const WmBusPacketPayloadData* payload;
    const WmBusPacketIdentityData* identity;
} WmBusParserPacketView;
