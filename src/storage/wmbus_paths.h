#pragma once

#include <stdbool.h>

#include <storage/storage.h>

#define WMBUS_APP_FOLDER            EXT_PATH("apps_data/wmbus_decoder")
#define WMBUS_SETTINGS_PATH         WMBUS_APP_FOLDER "/settings.txt"
#define WMBUS_KEYRING_PATH          WMBUS_APP_FOLDER "/keys.txt"
#define WMBUS_PACKET_LOG_PATH_MAX       96U
#define WMBUS_SELFTEST_REPORT_PATH      WMBUS_APP_FOLDER "/selftest.txt"

bool wmbus_storage_ensure_app_folder(Storage* storage);
