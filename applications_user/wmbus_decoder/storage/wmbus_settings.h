#pragma once

#include <stdbool.h>

#include <storage/storage.h>

#include "../core/wmbus_types.h"
#include "wmbus_paths.h"

typedef struct {
    WmBusRxMode mode;
    WmBusCsvLogging csv_logging;
    WmBusStatus memory_threshold;
    WmBusStatus csv_threshold;
    WmBusStatusMask memory_status_mask;
    WmBusStatusMask csv_status_mask;
    bool debug_overlay;
} WmBusSettings;

void wmbus_settings_reset(WmBusSettings* settings);
bool wmbus_settings_load(Storage* storage, WmBusSettings* settings);
bool wmbus_settings_save(Storage* storage, const WmBusSettings* settings);
