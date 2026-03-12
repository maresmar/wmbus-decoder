#include "../../app/wmbus_app_i.h"

static const char* const wmbus_mask_toggle[] = {
    "OFF",
    "ON",
};

typedef struct {
    WmBusApp* app;
    WmBusMaskTarget target;
    WmBusStatus status;
} WmBusMaskItemContext;

static WmBusMaskItemContext wmbus_mask_item_contexts[WmBusStatusCount];

static WmBusStatusMask* wmbus_scene_status_mask_get_mask(
    WmBusApp* app,
    WmBusMaskTarget target) {
    return (target == WmBusMaskTargetCsv) ? &app->settings.csv_status_mask :
                                            &app->settings.memory_status_mask;
}

static void wmbus_scene_status_mask_changed(VariableItem* item) {
    WmBusMaskItemContext* item_ctx = variable_item_get_context(item);
    if(!item_ctx) return;

    WmBusStatusMask* mask = wmbus_scene_status_mask_get_mask(item_ctx->app, item_ctx->target);
    uint8_t enabled = variable_item_get_current_value_index(item) ? 1U : 0U;
    variable_item_set_current_value_text(item, wmbus_mask_toggle[enabled]);

    if(enabled) {
        *mask |= WMBUS_STATUS_MASK(item_ctx->status);
    } else {
        *mask &= ~WMBUS_STATUS_MASK(item_ctx->status);
    }

    wmbus_app_apply_runtime_config(item_ctx->app, true);
}

void wmbus_scene_status_mask_on_enter(void* context) {
    WmBusApp* app = context;
    if(!wmbus_app_ensure_status_mask_view(app)) {
        return;
    }
    WmBusMaskTarget target =
        (WmBusMaskTarget)scene_manager_get_scene_state(app->scene_manager, WmBusSceneStatusMask);
    WmBusStatusMask mask = *wmbus_scene_status_mask_get_mask(app, target);

    VariableItem* item = variable_item_list_add(app->status_mask_list, "Target", 1U, NULL, NULL);
    variable_item_set_current_value_text(item, target == WmBusMaskTargetCsv ? "CSV" : "Memory");

    for(uint8_t status = WmBusStatusDecodeFail; status < WmBusStatusCount; status++) {
        wmbus_mask_item_contexts[status].app = app;
        wmbus_mask_item_contexts[status].target = target;
        wmbus_mask_item_contexts[status].status = (WmBusStatus)status;

        item = variable_item_list_add(
            app->status_mask_list,
            wmbus_packet_status_short_label((WmBusStatus)status),
            2U,
            wmbus_scene_status_mask_changed,
            &wmbus_mask_item_contexts[status]);
        variable_item_set_current_value_index(
            item, wmbus_status_mask_test(mask, (WmBusStatus)status) ? 1U : 0U);
        variable_item_set_current_value_text(
            item,
            wmbus_mask_toggle[wmbus_status_mask_test(mask, (WmBusStatus)status) ? 1U : 0U]);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, WmBusAppViewStatusMask);
}

bool wmbus_scene_status_mask_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void wmbus_scene_status_mask_on_exit(void* context) {
    WmBusApp* app = context;
    if(!app->status_mask_list) return;
    variable_item_list_set_selected_item(app->status_mask_list, 0U);
    variable_item_list_reset(app->status_mask_list);
}
