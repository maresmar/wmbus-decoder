#include "../../app/wmbus_app_i.h"

#include <string.h>

static void wmbus_scene_key_add_byte_input_callback(void* context) {
    WmBusApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, WmBusCustomEventConfigKeyInputDone);
}

void wmbus_scene_key_add_on_enter(void* context) {
    WmBusApp* app = context;
    if(!wmbus_app_ensure_key_input_view(app)) {
        return;
    }

    memset(app->key_input_bytes, 0, sizeof(app->key_input_bytes));

    byte_input_set_header_text(app->key_input, "Add 16-byte key");
    byte_input_set_result_callback(
        app->key_input,
        wmbus_scene_key_add_byte_input_callback,
        NULL,
        app,
        app->key_input_bytes,
        sizeof(app->key_input_bytes));

    view_dispatcher_switch_to_view(app->view_dispatcher, WmBusAppViewKeyInput);
}

bool wmbus_scene_key_add_on_event(void* context, SceneManagerEvent event) {
    WmBusApp* app = context;

    if(event.type == SceneManagerEventTypeCustom &&
       event.event == WmBusCustomEventConfigKeyInputDone) {
        wmbus_app_add_key(app, app->key_input_bytes);
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }

    return false;
}

void wmbus_scene_key_add_on_exit(void* context) {
    WmBusApp* app = context;
    if(!app->key_input) return;

    byte_input_set_result_callback(app->key_input, NULL, NULL, NULL, NULL, 0U);
    byte_input_set_header_text(app->key_input, "");
    memset(app->key_input_bytes, 0, sizeof(app->key_input_bytes));
}
