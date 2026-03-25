#pragma once

#include "sink/wmbus_packet_sink.h"

#include "../protocol/capture/wmbus_capture.h"
#include "../protocol/crypto/wmbus_crypto_key_store.h"
#include "../storage/wmbus_settings.h"

typedef struct WmBusCaptureProcessor WmBusCaptureProcessor;

WmBusCaptureProcessor* wmbus_capture_processor_alloc(void);
void wmbus_capture_processor_free(WmBusCaptureProcessor* processor);
bool wmbus_capture_processor_add_sink(
    WmBusCaptureProcessor* processor,
    const WmBusPacketSink* sink);
void wmbus_capture_processor_handle(
    WmBusCaptureProcessor* processor,
    const WmBusSettings* settings,
    const WmBusCryptoKeyStore* key_store,
    const WmBusCaptureFrame* capture);
