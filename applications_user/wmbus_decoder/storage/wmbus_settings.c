#include "wmbus_settings.h"

#include <flipper_format/flipper_format.h>

#define WMBUS_SETTINGS_FILE_TYPE "WM-Bus Decoder Settings"
#define WMBUS_SETTINGS_VERSION   2U

static WmBusStatus wmbus_settings_mask_to_threshold(
    WmBusStatusMask mask,
    WmBusStatus fallback) {
    for(WmBusStatus status = WmBusStatusDecodeFail; status < WmBusStatusCount; status++) {
        if(wmbus_status_mask_test(mask, status)) {
            return status;
        }
    }

    return fallback;
}

void wmbus_settings_reset(WmBusSettings* settings) {
    if(!settings) return;

    settings->mode = WmBusRxModeT;
    settings->csv_logging = WmBusCsvLoggingBasic;
    settings->memory_threshold = WmBusStatusDecodeFail;
    settings->csv_threshold = WmBusStatusCrcBad;
    settings->memory_status_mask = WMBUS_STATUS_MASK_ALL;
    settings->csv_status_mask = WMBUS_STATUS_MASK(WmBusStatusOk) |
                                WMBUS_STATUS_MASK(WmBusStatusWeakRssi) |
                                WMBUS_STATUS_MASK(WmBusStatusCrcBad);
    settings->debug_overlay = false;
}

bool wmbus_settings_load(Storage* storage, WmBusSettings* settings) {
    if(!storage || !settings) return false;

    wmbus_settings_reset(settings);
    wmbus_storage_ensure_app_folder(storage);

    FlipperFormat* fff = flipper_format_file_alloc(storage);
    bool loaded = false;
    FuriString* type = furi_string_alloc();
    uint32_t version = 0;
    uint32_t mode = 0;
    uint32_t csv_logging = 0;
    uint32_t memory_threshold = 0;
    uint32_t csv_threshold = 0;
    uint32_t memory_mask = 0;
    uint32_t csv_mask = 0;
    bool debug_overlay = false;

    if(flipper_format_file_open_existing(fff, WMBUS_SETTINGS_PATH)) {
        do {
            if(!flipper_format_read_header(fff, type, &version)) break;
            if(strcmp(furi_string_get_cstr(type), WMBUS_SETTINGS_FILE_TYPE) != 0) break;
            if(!flipper_format_read_uint32(fff, "mode", &mode, 1)) break;
            if(!flipper_format_read_uint32(fff, "csv_logging", &csv_logging, 1)) break;

            if(mode > WmBusRxModeC) break;
            if(csv_logging >= WmBusCsvLoggingCount) break;

            settings->mode = (WmBusRxMode)mode;
            settings->csv_logging = (WmBusCsvLogging)csv_logging;

            if(version >= 2U) {
                if(!flipper_format_read_uint32(fff, "memory_threshold", &memory_threshold, 1)) break;
                if(!flipper_format_read_uint32(fff, "csv_threshold", &csv_threshold, 1)) break;
                if(!flipper_format_read_bool(fff, "debug_overlay", &debug_overlay, 1)) break;

                settings->memory_threshold =
                    wmbus_status_threshold_clamp((WmBusStatus)memory_threshold);
                settings->csv_threshold =
                    wmbus_status_threshold_clamp((WmBusStatus)csv_threshold);
            } else if(version == 1U) {
                if(!flipper_format_read_uint32(fff, "memory_status_mask", &memory_mask, 1)) break;
                if(!flipper_format_read_uint32(fff, "csv_status_mask", &csv_mask, 1)) break;
                if(!flipper_format_read_bool(fff, "debug_overlay", &debug_overlay, 1)) break;

                settings->memory_status_mask = memory_mask ? memory_mask : WMBUS_STATUS_MASK_ALL;
                settings->csv_status_mask = csv_mask ? csv_mask : WMBUS_STATUS_MASK(WmBusStatusOk);
                settings->memory_threshold = wmbus_settings_mask_to_threshold(
                    settings->memory_status_mask, WmBusStatusDecodeFail);
                settings->csv_threshold = wmbus_settings_mask_to_threshold(
                    settings->csv_status_mask, WmBusStatusCrcBad);
            } else {
                break;
            }

            settings->memory_status_mask = WMBUS_STATUS_MASK_ALL;
            settings->csv_status_mask = WMBUS_STATUS_MASK(WmBusStatusOk) |
                                        WMBUS_STATUS_MASK(WmBusStatusWeakRssi) |
                                        WMBUS_STATUS_MASK(WmBusStatusCrcBad);
            settings->debug_overlay = debug_overlay;
            loaded = true;
        } while(false);
    }

    furi_string_free(type);
    flipper_format_free(fff);
    return loaded;
}

bool wmbus_settings_save(Storage* storage, const WmBusSettings* settings) {
    if(!storage || !settings) return false;
    if(!wmbus_storage_ensure_app_folder(storage)) return false;

    FlipperFormat* fff = flipper_format_file_alloc(storage);
    bool saved = false;

    if(flipper_format_file_open_always(fff, WMBUS_SETTINGS_PATH)) {
        do {
            uint32_t mode = settings->mode;
            uint32_t csv_logging = settings->csv_logging;
            uint32_t memory_threshold = settings->memory_threshold;
            uint32_t csv_threshold = settings->csv_threshold;
            bool debug_overlay = settings->debug_overlay;

            if(!flipper_format_write_header_cstr(
                   fff, WMBUS_SETTINGS_FILE_TYPE, WMBUS_SETTINGS_VERSION))
                break;
            if(!flipper_format_write_uint32(fff, "mode", &mode, 1)) break;
            if(!flipper_format_write_uint32(fff, "csv_logging", &csv_logging, 1)) break;
            if(!flipper_format_write_uint32(fff, "memory_threshold", &memory_threshold, 1)) break;
            if(!flipper_format_write_uint32(fff, "csv_threshold", &csv_threshold, 1)) break;
            if(!flipper_format_write_bool(fff, "debug_overlay", &debug_overlay, 1)) break;

            saved = true;
        } while(false);
    }

    flipper_format_free(fff);
    return saved;
}
