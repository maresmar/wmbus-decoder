#pragma once

#include "wmbus_app.h"
#include "wmbus_capture_processor.h"
#include "sink/wmbus_csv_sink.h"
#include "sink/wmbus_history_sink.h"
#include "radio/wmbus_radio_rx_service.h"

#include "../storage/wmbus_keyring.h"
#include "../storage/wmbus_settings.h"
#include "../ui/scenes/wmbus_scene.h"
#include "../ui/views/wmbus_rx_view.h"

#include <gui/gui.h>
#include <gui/modules/byte_input.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <storage/storage.h>

typedef enum {
    WmBusAppViewRx = 0,
    WmBusAppViewConfig,
    WmBusAppViewKeyInput,
    WmBusAppViewStatusMask,
    WmBusAppViewPacketDetail,
} WmBusAppView;

typedef enum {
    WmBusMaskTargetMemory = 0,
    WmBusMaskTargetCsv,
} WmBusMaskTarget;

typedef enum {
    WmBusCustomEventOpenConfig = WmBusRxViewEventOpenConfig,
    WmBusCustomEventOpenDetails = WmBusRxViewEventOpenDetails,
    WmBusCustomEventToggleDebug = WmBusRxViewEventToggleDebug,
    WmBusCustomEventConfigMemoryMask = 100,
    WmBusCustomEventConfigCsvMask,
    WmBusCustomEventConfigOpenKeyInput,
    WmBusCustomEventConfigKeyInputDone,
} WmBusCustomEvent;

typedef struct WmBusApp WmBusApp;

struct WmBusApp {
    Gui* gui;
    Storage* storage;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    VariableItemList* config_list;
    ByteInput* key_input;
    VariableItemList* status_mask_list;
    Widget* detail_widget;
    WmBusRxView* rx_view;

    WmBusCsvSink csv_sink;
    WmBusHistorySink history_sink;
    WmBusCaptureProcessor* capture_processor;
    WmBusRadioRxService* rx_service;
    FuriMutex* keyring_mutex;

    WmBusSettings settings;
    WmBusKeyring keyring;
    WmBusMaskTarget mask_target;
    uint8_t key_input_bytes[WMBUS_KEY_BYTES];
};

bool wmbus_app_apply_runtime_config(WmBusApp* app, bool persist);
bool wmbus_app_reload_keys(WmBusApp* app);
bool wmbus_app_add_key(WmBusApp* app, const uint8_t key[WMBUS_KEY_BYTES]);
bool wmbus_app_ensure_config_view(WmBusApp* app);
bool wmbus_app_ensure_key_input_view(WmBusApp* app);
bool wmbus_app_ensure_status_mask_view(WmBusApp* app);
bool wmbus_app_ensure_detail_view(WmBusApp* app);
