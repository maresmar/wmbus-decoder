#pragma once

#include "../protocol/capture/wmbus_capture.h"
#include "../protocol/crypto/wmbus_crypto_key_store.h"
#include "../storage/wmbus_settings.h"
#include "../ui/views/wmbus_rx_view.h"

#include <storage/storage.h>

typedef struct WmBusCaptureProcessor WmBusCaptureProcessor;

WmBusCaptureProcessor* wmbus_capture_processor_alloc(Storage* storage, WmBusRxView* rx_view);
void wmbus_capture_processor_free(WmBusCaptureProcessor* processor);
void wmbus_capture_processor_handle(
    WmBusCaptureProcessor* processor,
    const WmBusSettings* settings,
    const WmBusCryptoKeyStore* key_store,
    const WmBusCaptureFrame* capture);
