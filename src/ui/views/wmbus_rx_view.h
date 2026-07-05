#pragma once

#include <stdbool.h>
#include <stddef.h>

#include <furi.h>
#include <gui/view_dispatcher.h>

#include "../../protocol/packet/wmbus_packet.h"
#include "../../storage/wmbus_settings.h"

typedef struct WmBusRxView WmBusRxView;

typedef enum {
    WmBusRxViewEventOpenConfig = 1,
    WmBusRxViewEventOpenDetails,
    WmBusRxViewEventToggleDebug,
} WmBusRxViewEvent;

WmBusRxView* wmbus_rx_view_alloc(void);
void wmbus_rx_view_free(WmBusRxView* rx_view);
View* wmbus_rx_view_get_view(WmBusRxView* rx_view);
void wmbus_rx_view_set_dispatcher(WmBusRxView* rx_view, ViewDispatcher* view_dispatcher);
void wmbus_rx_view_apply_settings(WmBusRxView* rx_view, const WmBusSettings* settings);
void wmbus_rx_view_set_freq_valid(WmBusRxView* rx_view, bool freq_valid);
void wmbus_rx_view_set_live_rssi(WmBusRxView* rx_view, int rssi);
void wmbus_rx_view_push_packet(
    WmBusRxView* rx_view,
    const WmBusPacketRecord* record,
    bool rssi_gate_ok,
    bool store_in_history);
bool wmbus_rx_view_has_selected_packet(WmBusRxView* rx_view);
bool wmbus_rx_view_build_selected_detail_text(WmBusRxView* rx_view, FuriString* out);
