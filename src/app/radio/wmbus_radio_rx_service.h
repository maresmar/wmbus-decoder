#pragma once

#include "../../protocol/capture/wmbus_capture.h"
#include "../../protocol/crypto/wmbus_crypto_key_store.h"
#include "../../storage/wmbus_settings.h"

typedef struct WmBusRadioRxService WmBusRadioRxService;

typedef struct {
    void* context;
    void (*handle_capture)(
        void* context,
        const WmBusSettings* settings,
        const WmBusCryptoKeyStore* key_store,
        const WmBusCaptureFrame* capture);
    void (*set_freq_valid)(void* context, bool freq_valid);
    void (*set_live_rssi)(void* context, int rssi);
} WmBusRadioRxCallbacks;

WmBusRadioRxService* wmbus_radio_rx_service_alloc(
    const WmBusRadioRxCallbacks* callbacks,
    const WmBusSettings* settings,
    const WmBusCryptoKeyStore* key_store);

void wmbus_radio_rx_service_free(WmBusRadioRxService* service);

bool wmbus_radio_rx_service_apply_config(
    WmBusRadioRxService* service,
    const WmBusSettings* settings,
    const WmBusCryptoKeyStore* key_store);
