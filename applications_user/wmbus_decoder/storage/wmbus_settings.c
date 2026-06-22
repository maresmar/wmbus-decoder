#include "wmbus_settings.h"

#include <flipper_format/flipper_format.h>

#define WMBUS_SETTINGS_FILE_TYPE "WM-Bus Decoder Settings"
#define WMBUS_SETTINGS_VERSION   3U

static int32_t wmbus_settings_normalize_min_rssi(int32_t min_rssi_dbm) {
    if(min_rssi_dbm >= 0) return 0;

    int32_t magnitude = -min_rssi_dbm;
    int32_t rounded = -(((magnitude + 2) / 5) * 5);
    if(rounded < -100) return -100;
    if(rounded > -50) return -50;
    return rounded;
}

void wmbus_settings_reset(WmBusSettings* settings) {
    if(!settings) return;

    settings->mode = WmBusRxModeT;
    settings->csv_logging = WmBusCsvLoggingBasic;
    settings->min_rssi_dbm = 0;
    settings->memory_quality = WmBusPacketQualityAnyCapture;
    settings->csv_quality = WmBusPacketQualityCrcOk;
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
    int32_t min_rssi_dbm = 0;
    uint32_t memory_quality = 0;
    uint32_t csv_quality = 0;
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

            if(version >= 3U) {
                if(!flipper_format_read_int32(fff, "min_rssi_dbm", &min_rssi_dbm, 1)) break;
                if(!flipper_format_read_uint32(fff, "memory_quality", &memory_quality, 1)) break;
                if(!flipper_format_read_uint32(fff, "csv_quality", &csv_quality, 1)) break;
                if(!flipper_format_read_bool(fff, "debug_overlay", &debug_overlay, 1)) break;

                settings->min_rssi_dbm = wmbus_settings_normalize_min_rssi(min_rssi_dbm);
                settings->memory_quality =
                    wmbus_packet_quality_clamp((WmBusPacketQuality)memory_quality);
                settings->csv_quality =
                    wmbus_packet_quality_clamp((WmBusPacketQuality)csv_quality);
            } else if(version == 2U) {
                uint32_t legacy_threshold = 0;
                if(!flipper_format_read_uint32(fff, "memory_threshold", &legacy_threshold, 1)) break;
                if(!flipper_format_read_uint32(fff, "csv_threshold", &legacy_threshold, 1)) break;
                if(!flipper_format_read_bool(fff, "debug_overlay", &debug_overlay, 1)) break;
            } else if(version == 1U) {
                uint32_t legacy_mask = 0;
                if(!flipper_format_read_uint32(fff, "memory_status_mask", &legacy_mask, 1)) break;
                if(!flipper_format_read_uint32(fff, "csv_status_mask", &legacy_mask, 1)) break;
                if(!flipper_format_read_bool(fff, "debug_overlay", &debug_overlay, 1)) break;
            } else {
                break;
            }

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
            int32_t min_rssi_dbm = wmbus_settings_normalize_min_rssi(settings->min_rssi_dbm);
            uint32_t memory_quality = settings->memory_quality;
            uint32_t csv_quality = settings->csv_quality;
            bool debug_overlay = settings->debug_overlay;

            if(!flipper_format_write_header_cstr(
                   fff, WMBUS_SETTINGS_FILE_TYPE, WMBUS_SETTINGS_VERSION))
                break;
            if(!flipper_format_write_uint32(fff, "mode", &mode, 1)) break;
            if(!flipper_format_write_uint32(fff, "csv_logging", &csv_logging, 1)) break;
            if(!flipper_format_write_int32(fff, "min_rssi_dbm", &min_rssi_dbm, 1)) break;
            if(!flipper_format_write_uint32(fff, "memory_quality", &memory_quality, 1)) break;
            if(!flipper_format_write_uint32(fff, "csv_quality", &csv_quality, 1)) break;
            if(!flipper_format_write_bool(fff, "debug_overlay", &debug_overlay, 1)) break;

            saved = true;
        } while(false);
    }

    flipper_format_free(fff);
    return saved;
}
