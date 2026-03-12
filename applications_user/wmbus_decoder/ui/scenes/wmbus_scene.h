#pragma once

#include <gui/scene_manager.h>

#define ADD_SCENE(prefix, name, id) WmBusScene##id,
typedef enum {
#include "wmbus_scene_config.h"
    WmBusSceneNum,
} WmBusScene;
#undef ADD_SCENE

extern const SceneManagerHandlers wmbus_scene_handlers;

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "wmbus_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "wmbus_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void*);
#include "wmbus_scene_config.h"
#undef ADD_SCENE
