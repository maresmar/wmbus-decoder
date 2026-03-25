#pragma once

#include "../wmbus_capture_processor.h"

#include "../../storage/wmbus_keyring.h"
#include "../../storage/wmbus_settings.h"
#include "../../ui/views/wmbus_rx_view.h"

typedef struct WmBusRadioRxService WmBusRadioRxService;

WmBusRadioRxService* wmbus_radio_rx_service_alloc(
    WmBusCaptureProcessor* capture_processor,
    WmBusRxView* rx_view,
    const WmBusSettings* settings,
    FuriMutex* keyring_mutex,
    const WmBusKeyring* keyring);

void wmbus_radio_rx_service_free(WmBusRadioRxService* service);

bool wmbus_radio_rx_service_apply_config(
    WmBusRadioRxService* service,
    const WmBusSettings* settings);
