#include "wmbus_log.h"
#include "wmbus_paths.h"

#include "../protocol/format/wmbus_packet_summary.h"
#include "../protocol/format/wmbus_record_formatter.h"
#include "../protocol/model/wmbus_application_record.h"
#include "../protocol/parser/wmbus_parser.h"

#include <furi.h>
#include <furi_hal.h>
#include <stdarg.h>
#include <stdio.h>
#define WMBUS_LOG_LINE_MAX 1024U

static const char* wmbus_log_header(WmBusCsvLogging logging) {
    return (logging == WmBusCsvLoggingFull) ?
               "tick,mode,status,quality,flags,normalize_format,crc_ok,mfg,id,version,device_type,ci,rssi,parser,"
               "security_mode,decrypted,key_index,total_m3,fields,capture_hex,packet_hex\n" :
               "tick,mode,status,quality,mfg,id,version,device_type,ci,rssi,parser,total_m3\n";
}

static void wmbus_log_format_path(WmBusCsvLogging logging, char* path, size_t path_size) {
    if(!path || path_size == 0U) return;

    DateTime now = {0};
    furi_hal_rtc_get_datetime(&now);
    const char* kind = (logging == WmBusCsvLoggingFull) ? "full" : "basic";

    if(now.year >= 2020U && now.month >= 1U && now.month <= 12U && now.day >= 1U &&
       now.day <= 31U) {
        snprintf(
            path,
            path_size,
            "%s/packets_%04u%02u%02u_%s.csv",
            WMBUS_APP_FOLDER,
            (unsigned int)now.year,
            (unsigned int)now.month,
            (unsigned int)now.day,
            kind);
    } else {
        snprintf(path, path_size, "%s/packets_undated_%s.csv", WMBUS_APP_FOLDER, kind);
    }
}

static bool wmbus_log_write_line(File* file, const char* format, ...) {
    if(!file || !format) return false;

    char line[WMBUS_LOG_LINE_MAX];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(line, sizeof(line), format, args);
    va_end(args);

    if(len < 0) return false;

    size_t to_write = (size_t)len;
    if(to_write >= sizeof(line)) {
        to_write = sizeof(line) - 1U;
    }

    return storage_file_write(file, line, to_write) == to_write;
}

static void
    wmbus_log_format_hex(const uint8_t* data, size_t data_len, char* out, size_t out_size) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';

    if(!data) return;

    size_t write = 0;
    for(size_t i = 0; i < data_len && (write + 2U) < out_size; i++) {
        snprintf(&out[write], out_size - write, "%02X", data[i]);
        write += 2U;
    }
    out[write] = '\0';
}

static void
    wmbus_log_format_total_m3(const WmBusPacketRecord* record, char* out, size_t out_size) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';
    if(!record) return;

    uint32_t total_m3_x1000 = 0U;
    if(!wmbus_packet_summary_find_total_m3(&record->application, &total_m3_x1000)) {
        return;
    }
    wmbus_packet_summary_format_total_m3(total_m3_x1000, out, out_size, false);
}

static const char* wmbus_log_normalize_format(const WmBusPacketRecord* record) {
    if(!record || !record->normalize_format_known) {
        return "";
    }

    switch(record->normalize_format) {
    case WmBusFrameFormatA:
        return "A";
    case WmBusFrameFormatB:
        return "B";
    default:
        furi_crash("Unknown frame format");
    }
}

static uint8_t wmbus_log_security_mode(const WmBusPacketRecord* record) {
    if(!record) {
        return 0U;
    }
    if(record->ell.has_ell && record->ell.has_session) {
        return record->ell.security_mode;
    }
    return record->tpl.security_mode;
}

static const char* wmbus_log_decrypted(const WmBusPacketRecord* record) {
    if(!record) {
        return "no";
    }
    if(record->ell.has_ell && record->ell.has_session) {
        return record->ell.decrypted ? "yes" : "no";
    }
    return record->tpl.decrypted ? "yes" : "no";
}

static uint8_t wmbus_log_key_index(const WmBusPacketRecord* record) {
    if(!record) {
        return 0U;
    }
    if(record->ell.has_ell && record->ell.has_session) {
        return record->ell.key_index;
    }
    return record->tpl.key_index;
}

static void wmbus_log_format_flags(const WmBusPacketRecord* record, char* out, size_t out_size) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';
    if(!record) return;

    snprintf(
        out,
        out_size,
        "%s;%s;%s;%s;%s;%s",
        record->has_capture ? "RX" : "",
        record->header_ok ? "HDR" : "",
        record->length_ok ? "LEN" : "",
        (record->crc_known && record->crc_ok) ? "CRC" : "",
        record->parsed_ok ? "DECODED" : "",
        record->rssi_ok ? "RSSI" : "RSSI_WEAK");
}

bool wmbus_log_append(Storage* storage, WmBusCsvLogging logging, const WmBusPacketRecord* record) {
    if(!storage || !record || logging == WmBusCsvLoggingNone) return false;

    if(!wmbus_storage_ensure_app_folder(storage)) {
        return false;
    }

    File* file = storage_file_alloc(storage);
    bool written = false;

    do {
        char path[WMBUS_PACKET_LOG_PATH_MAX] = {0};
        wmbus_log_format_path(logging, path, sizeof(path));
        if(!storage_file_open(file, path, FSAM_READ_WRITE, FSOM_OPEN_APPEND)) {
            break;
        }

        if(storage_file_size(file) == 0U) {
            if(!wmbus_log_write_line(file, "%s", wmbus_log_header(logging))) {
                break;
            }
        }

        char packet_hex[513] = {0};
        char capture_hex[513] = {0};
        char total_m3[24] = {0};
        char flags[48] = {0};
        FuriString* fields = furi_string_alloc();
        if(!fields) {
            break;
        }
        wmbus_record_formatter_format_joined(
            record->application.records, record->application.record_count, ';', fields);
        if(record->crc_known && record->crc_ok) {
            wmbus_log_format_hex(
                record->packet_bytes, record->packet_len, packet_hex, sizeof(packet_hex));
        } else {
            wmbus_log_format_hex(
                record->capture_bytes, record->capture_len, capture_hex, sizeof(capture_hex));
        }
        wmbus_log_format_total_m3(record, total_m3, sizeof(total_m3));
        wmbus_log_format_flags(record, flags, sizeof(flags));

        if(logging == WmBusCsvLoggingFull) {
            written = wmbus_log_write_line(
                file,
                "%lu,%c,%s,%s,%s,%s,%s,%s,%s,%02X,%02X,%02X,%d,%s,%02X,%s,%u,%s,%s,%s,%s\n",
                (unsigned long)record->rx_tick,
                record->mode == WmBusRxModeT ? 'T' : 'C',
                wmbus_packet_status_str(record->status),
                wmbus_packet_quality_str(record->quality),
                flags,
                wmbus_log_normalize_format(record),
                record->crc_known ? (record->crc_ok ? "yes" : "no") : "",
                record->packet_is_frame ? record->identity.manufacturer : "",
                record->packet_is_frame ? record->identity.meter_id : "",
                record->packet_is_frame ? record->dll.version : 0U,
                record->packet_is_frame ? record->dll.dev_type : 0U,
                record->packet_is_frame ? record->dll.ci_field : 0U,
                record->rssi,
                wmbus_parser_id_name(record->application.parser_id),
                wmbus_log_security_mode(record),
                wmbus_log_decrypted(record),
                wmbus_log_key_index(record),
                total_m3,
                furi_string_get_cstr(fields),
                capture_hex,
                packet_hex);
        } else {
            written = wmbus_log_write_line(
                file,
                "%lu,%c,%s,%s,%s,%s,%02X,%02X,%02X,%d,%s,%s\n",
                (unsigned long)record->rx_tick,
                record->mode == WmBusRxModeT ? 'T' : 'C',
                wmbus_packet_status_str(record->status),
                wmbus_packet_quality_str(record->quality),
                record->packet_is_frame ? record->identity.manufacturer : "",
                record->packet_is_frame ? record->identity.meter_id : "",
                record->packet_is_frame ? record->dll.version : 0U,
                record->packet_is_frame ? record->dll.dev_type : 0U,
                record->packet_is_frame ? record->dll.ci_field : 0U,
                record->rssi,
                wmbus_parser_id_name(record->application.parser_id),
                total_m3);
        }

        furi_string_free(fields);

        storage_file_sync(file);
    } while(false);

    if(storage_file_is_open(file)) {
        storage_file_close(file);
    }
    storage_file_free(file);
    return written;
}
