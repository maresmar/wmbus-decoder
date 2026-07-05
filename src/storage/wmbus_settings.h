#pragma once

#include <stdbool.h>

#include <storage/storage.h>

#include "../core/wmbus_types.h"
#include "wmbus_paths.h"

typedef struct {
    WmBusRxMode mode;
    WmBusCsvLogging csv_logging;
    int32_t min_rssi_dbm;
    WmBusPacketQuality memory_quality;
    WmBusPacketQuality csv_quality;
    bool debug_overlay;
} WmBusSettings;

void wmbus_settings_reset(WmBusSettings* settings);
bool wmbus_settings_load(Storage* storage, WmBusSettings* settings);
bool wmbus_settings_save(Storage* storage, const WmBusSettings* settings);
