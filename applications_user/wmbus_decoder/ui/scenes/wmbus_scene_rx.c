#include "../../app/wmbus_app_i.h"

void wmbus_scene_rx_on_enter(void* context) {
    WmBusApp* app = context;
    view_dispatcher_switch_to_view(app->view_dispatcher, WmBusAppViewRx);
}

bool wmbus_scene_rx_on_event(void* context, SceneManagerEvent event) {
    WmBusApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        switch(event.event) {
        case WmBusCustomEventOpenConfig:
            scene_manager_next_scene(app->scene_manager, WmBusSceneConfig);
            break;
        case WmBusCustomEventOpenDetails:
            if(wmbus_rx_view_has_selected_packet(app->rx_view)) {
                scene_manager_next_scene(app->scene_manager, WmBusScenePacketDetail);
            }
            break;
        case WmBusCustomEventToggleDebug:
            app->settings.debug_overlay = !app->settings.debug_overlay;
            wmbus_app_apply_runtime_config(app, true);
            break;
        default:
            consumed = false;
            break;
        }
    }

    return consumed;
}

void wmbus_scene_rx_on_exit(void* context) {
    UNUSED(context);
}
