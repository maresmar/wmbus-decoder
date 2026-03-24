#include "../../app/wmbus_app_i.h"

void wmbus_scene_rx_on_enter(void* context) {
    WmBusApp* app = context;
    view_dispatcher_switch_to_view(app->view_dispatcher, WmBusAppViewRx);
}

bool wmbus_scene_rx_on_event(void* context, SceneManagerEvent event) {
    WmBusApp* app = context;

    if(event.type != SceneManagerEventTypeCustom) {
        return false;
    }

    switch(event.event) {
    case WmBusCustomEventOpenConfig:
        scene_manager_next_scene(app->scene_manager, WmBusSceneConfig);
        return true;
    case WmBusCustomEventOpenDetails:
        if(!wmbus_rx_view_has_selected_packet(app->rx_view)) {
            return false;
        }
        scene_manager_next_scene(app->scene_manager, WmBusScenePacketDetail);
        return true;
    case WmBusCustomEventToggleDebug:
        app->settings.debug_overlay = !app->settings.debug_overlay;
        wmbus_app_apply_runtime_config(app, true);
        return true;
    default:
        return false;
    }
}

void wmbus_scene_rx_on_exit(void* context) {
    UNUSED(context);
}
