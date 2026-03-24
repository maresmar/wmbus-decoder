#include "../../app/wmbus_app_i.h"

enum WmBusConfigItem {
    WmBusConfigItemMode = 0,
    WmBusConfigItemCsvLogging,
    WmBusConfigItemMemoryThreshold,
    WmBusConfigItemCsvThreshold,
    WmBusConfigItemDebugOverlay,
    WmBusConfigItemKeyring,
};

static const char* const wmbus_mode_text[] = {
    "T",
    "C",
};

static const char* const wmbus_csv_logging_text[] = {
    "None",
    "Basic",
    "Full",
};

static const char* const wmbus_toggle_text[] = {
    "Off",
    "On",
};

static const char* const wmbus_status_threshold_text[] = {
    "Decode fail",
    "Not plausible",
    "Framing error",
    "CRC bad",
    "Weak RSSI",
    "OK",
};

static uint8_t wmbus_status_threshold_index(WmBusStatus status) {
    status = wmbus_status_threshold_clamp(status);
    return (uint8_t)(status - WmBusStatusDecodeFail);
}

static WmBusStatus wmbus_status_threshold_from_index(uint8_t index) {
    if(index >= COUNT_OF(wmbus_status_threshold_text)) {
        index = 0U;
    }

    return (WmBusStatus)(WmBusStatusDecodeFail + index);
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

static void wmbus_scene_config_memory_threshold_changed(VariableItem* item) {
    WmBusApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= COUNT_OF(wmbus_status_threshold_text)) {
        index = 0U;
    }
    variable_item_set_current_value_text(item, wmbus_status_threshold_text[index]);
    app->settings.memory_threshold = wmbus_status_threshold_from_index(index);
    wmbus_app_apply_runtime_config(app, true);
}

static void wmbus_scene_config_csv_threshold_changed(VariableItem* item) {
    WmBusApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= COUNT_OF(wmbus_status_threshold_text)) {
        index = 0U;
    }
    variable_item_set_current_value_text(item, wmbus_status_threshold_text[index]);
    app->settings.csv_threshold = wmbus_status_threshold_from_index(index);
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

static void wmbus_scene_config_add_threshold_item(
    WmBusApp* app,
    const char* label,
    WmBusStatus current,
    VariableItemChangeCallback callback) {
    VariableItem* item = variable_item_list_add(
        app->config_list,
        label,
        COUNT_OF(wmbus_status_threshold_text),
        callback,
        app);
    uint8_t index = wmbus_status_threshold_index(current);
    variable_item_set_current_value_index(item, index);
    variable_item_set_current_value_text(item, wmbus_status_threshold_text[index]);
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
    wmbus_scene_config_add_threshold_item(
        app,
        "Store if >=",
        app->settings.memory_threshold,
        wmbus_scene_config_memory_threshold_changed);
    wmbus_scene_config_add_threshold_item(
        app,
        "Log if >=",
        app->settings.csv_threshold,
        wmbus_scene_config_csv_threshold_changed);
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
