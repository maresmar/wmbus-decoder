#pragma once

#include <stdbool.h>

#include <storage/storage.h>

#include "../protocol/packet/wmbus_packet.h"

bool wmbus_log_append(
    Storage* storage,
    WmBusCsvLogging logging,
    const WmBusPacketRecord* record);
