#include "wmbus_paths.h"

bool wmbus_storage_ensure_app_folder(Storage* storage) {
    if(!storage) return false;
    return storage_simply_mkdir(storage, WMBUS_APP_FOLDER);
}
