#pragma once

#include "../protocol/capture/wmbus_capture.h"
#include "../storage/wmbus_keyring.h"
#include "../storage/wmbus_settings.h"
#include "../ui/views/wmbus_rx_view.h"

#include <storage/storage.h>

void wmbus_capture_processor_handle(
    Storage* storage,
    WmBusRxView* rx_view,
    const WmBusSettings* settings,
    const WmBusKeyring* keyring,
    const WmBusCaptureFrame* capture);
