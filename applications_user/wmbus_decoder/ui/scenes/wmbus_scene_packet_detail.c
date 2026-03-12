#include "../../app/wmbus_app_i.h"

void wmbus_scene_packet_detail_on_enter(void* context) {
    WmBusApp* app = context;
    if(!wmbus_app_ensure_detail_view(app)) {
        return;
    }
    char detail[WMBUS_PACKET_DETAIL_MAX];

    widget_reset(app->detail_widget);
    if(!wmbus_app_build_detail_text(app, detail, sizeof(detail))) {
        snprintf(detail, sizeof(detail), "No packet selected.");
    }
    widget_add_text_scroll_element(app->detail_widget, 0U, 0U, 128U, 64U, detail);
    view_dispatcher_switch_to_view(app->view_dispatcher, WmBusAppViewPacketDetail);
}

bool wmbus_scene_packet_detail_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void wmbus_scene_packet_detail_on_exit(void* context) {
    WmBusApp* app = context;
    if(!app->detail_widget) return;
    widget_reset(app->detail_widget);
}
