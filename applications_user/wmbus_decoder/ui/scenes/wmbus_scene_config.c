#include "../../app/wmbus_app_i.h"

enum WmBusConfigItem {
    WmBusConfigItemMode = 0,
    WmBusConfigItemCsvLogging,
    WmBusConfigItemMemoryQuality,
    WmBusConfigItemCsvQuality,
    WmBusConfigItemMinRssi,
    WmBusConfigItemDebugOverlay,
    WmBusConfigItemKeyring,
};

static const char* const wmbus_mode_text[] = {
    "T",
    "C",
};

static const char* const wmbus_csv_logging_text[] = {
    "Off",
    "Basic",
    "Full",
};

static const char* const wmbus_toggle_text[] = {
    "Off",
    "On",
};

static const char* const wmbus_quality_text[] = {
    "RX",
    "HDR OK",
    "LEN OK",
    "CRC OK",
    "DECODED",
};

static const int32_t wmbus_min_rssi_values[] = {
    0, -100, -95, -90, -85, -80, -75, -70, -65, -60, -55, -50,
};

static const char* const wmbus_min_rssi_text[] = {
    "Off", "-100 dBm", "-95 dBm", "-90 dBm", "-85 dBm", "-80 dBm",
    "-75 dBm", "-70 dBm", "-65 dBm", "-60 dBm", "-55 dBm", "-50 dBm",
};

static uint8_t wmbus_min_rssi_index(int32_t min_rssi_dbm) {
    for(uint8_t i = 0; i < COUNT_OF(wmbus_min_rssi_values); i++) {
        if(wmbus_min_rssi_values[i] == min_rssi_dbm) return i;
    }
    return 0U;
}

static void wmbus_scene_config_enter_callback(void* context, uint32_t index) {
    WmBusApp* app = context;

    if(index == WmBusConfigItemKeyring) {
        view_dispatcher_send_custom_event(app->view_dispatcher, WmBusCustomEventConfigOpenKeyInput);
    }
}

static void wmbus_scene_config_mode_changed(VariableItem* item) {
    WmBusApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index > 1U) index = 0U;
    variable_item_set_current_value_text(item, wmbus_mode_text[index]);
    app->settings.mode = (index == 1U) ? WmBusRxModeC : WmBusRxModeT;
    wmbus_app_apply_runtime_config(app, true);
}

static void wmbus_scene_config_csv_logging_changed(VariableItem* item) {
    WmBusApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= WmBusCsvLoggingCount) index = WmBusCsvLoggingNone;
    variable_item_set_current_value_text(item, wmbus_csv_logging_text[index]);
    app->settings.csv_logging = (WmBusCsvLogging)index;
    wmbus_app_apply_runtime_config(app, true);
}

static void
    wmbus_scene_config_quality_changed(VariableItem* item, WmBusPacketQuality* quality) {
    WmBusApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= COUNT_OF(wmbus_quality_text)) {
        index = 0U;
    }
    variable_item_set_current_value_text(item, wmbus_quality_text[index]);
    *quality = (WmBusPacketQuality)index;
    wmbus_app_apply_runtime_config(app, true);
}

static void wmbus_scene_config_memory_quality_changed(VariableItem* item) {
    WmBusApp* app = variable_item_get_context(item);
    wmbus_scene_config_quality_changed(item, &app->settings.memory_quality);
}

static void wmbus_scene_config_csv_quality_changed(VariableItem* item) {
    WmBusApp* app = variable_item_get_context(item);
    wmbus_scene_config_quality_changed(item, &app->settings.csv_quality);
}

static void wmbus_scene_config_min_rssi_changed(VariableItem* item) {
    WmBusApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= COUNT_OF(wmbus_min_rssi_values)) {
        index = 0U;
    }
    variable_item_set_current_value_text(item, wmbus_min_rssi_text[index]);
    app->settings.min_rssi_dbm = wmbus_min_rssi_values[index];
    wmbus_app_apply_runtime_config(app, true);
}

static void wmbus_scene_config_debug_overlay_changed(VariableItem* item) {
    WmBusApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index > 1U) index = 0U;
    variable_item_set_current_value_text(item, wmbus_toggle_text[index]);
    app->settings.debug_overlay = (index != 0U);
    wmbus_app_apply_runtime_config(app, true);
}

static void wmbus_scene_config_add_mode_item(WmBusApp* app) {
    VariableItem* item = variable_item_list_add(
        app->config_list, "Mode", COUNT_OF(wmbus_mode_text), wmbus_scene_config_mode_changed, app);
    uint8_t index = (app->settings.mode == WmBusRxModeC) ? 1U : 0U;
    variable_item_set_current_value_index(item, index);
    variable_item_set_current_value_text(item, wmbus_mode_text[index]);
}

static void wmbus_scene_config_add_csv_logging_item(WmBusApp* app) {
    VariableItem* item = variable_item_list_add(
        app->config_list,
        "CSV logging",
        WmBusCsvLoggingCount,
        wmbus_scene_config_csv_logging_changed,
        app);
    variable_item_set_current_value_index(item, app->settings.csv_logging);
    variable_item_set_current_value_text(item, wmbus_csv_logging_text[app->settings.csv_logging]);
}

static void wmbus_scene_config_add_quality_item(
    WmBusApp* app,
    const char* label,
    WmBusPacketQuality current,
    VariableItemChangeCallback callback) {
    VariableItem* item = variable_item_list_add(
        app->config_list,
        label,
        COUNT_OF(wmbus_quality_text),
        callback,
        app);
    uint8_t index = (uint8_t)wmbus_packet_quality_clamp(current);
    variable_item_set_current_value_index(item, index);
    variable_item_set_current_value_text(item, wmbus_quality_text[index]);
}

static void wmbus_scene_config_add_min_rssi_item(WmBusApp* app) {
    VariableItem* item = variable_item_list_add(
        app->config_list,
        "RSSI gate >=",
        COUNT_OF(wmbus_min_rssi_values),
        wmbus_scene_config_min_rssi_changed,
        app);
    uint8_t index = wmbus_min_rssi_index(app->settings.min_rssi_dbm);
    variable_item_set_current_value_index(item, index);
    variable_item_set_current_value_text(item, wmbus_min_rssi_text[index]);
}

static void wmbus_scene_config_add_debug_overlay_item(WmBusApp* app) {
    VariableItem* item = variable_item_list_add(
        app->config_list,
        "Debug overlay",
        COUNT_OF(wmbus_toggle_text),
        wmbus_scene_config_debug_overlay_changed,
        app);
    uint8_t index = app->settings.debug_overlay ? 1U : 0U;
    variable_item_set_current_value_index(item, index);
    variable_item_set_current_value_text(item, wmbus_toggle_text[index]);
}

static void wmbus_scene_config_add_keyring_item(WmBusApp* app) {
    VariableItem* item = variable_item_list_add(app->config_list, "Keyring", 1U, NULL, app);
    variable_item_set_current_value_text(item, app->keyring.status);
}

void wmbus_scene_config_on_enter(void* context) {
    WmBusApp* app = context;
    if(!wmbus_app_ensure_config_view(app)) {
        return;
    }

    variable_item_list_set_enter_callback(
        app->config_list, wmbus_scene_config_enter_callback, app);
    wmbus_scene_config_add_mode_item(app);
    wmbus_scene_config_add_csv_logging_item(app);
    wmbus_scene_config_add_quality_item(
        app,
        "MEM gate >=",
        app->settings.memory_quality,
        wmbus_scene_config_memory_quality_changed);
    wmbus_scene_config_add_quality_item(
        app,
        "CSV gate >=",
        app->settings.csv_quality,
        wmbus_scene_config_csv_quality_changed);
    wmbus_scene_config_add_min_rssi_item(app);
    wmbus_scene_config_add_debug_overlay_item(app);
    wmbus_scene_config_add_keyring_item(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, WmBusAppViewConfig);
}

bool wmbus_scene_config_on_event(void* context, SceneManagerEvent event) {
    WmBusApp* app = context;

    if(event.type != SceneManagerEventTypeCustom) {
        return false;
    }

    switch(event.event) {
    case WmBusCustomEventConfigOpenKeyInput:
        scene_manager_next_scene(app->scene_manager, WmBusSceneKeyAdd);
        return true;
    default:
        return false;
    }
}

void wmbus_scene_config_on_exit(void* context) {
    WmBusApp* app = context;
    if(!app->config_list) return;
    variable_item_list_set_selected_item(app->config_list, 0U);
    variable_item_list_reset(app->config_list);
}
