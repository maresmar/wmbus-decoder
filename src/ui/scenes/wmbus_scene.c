#include "wmbus_scene.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const wmbus_scene_on_enter_handlers[])(void*) = {
#include "wmbus_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const wmbus_scene_on_event_handlers[])(void*, SceneManagerEvent) = {
#include "wmbus_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const wmbus_scene_on_exit_handlers[])(void*) = {
#include "wmbus_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers wmbus_scene_handlers = {
    .on_enter_handlers = wmbus_scene_on_enter_handlers,
    .on_event_handlers = wmbus_scene_on_event_handlers,
    .on_exit_handlers = wmbus_scene_on_exit_handlers,
    .scene_num = WmBusSceneNum,
};
