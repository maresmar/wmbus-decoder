#include "wmbus_log.h"
#include "wmbus_paths.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#define WMBUS_LOG_LINE_MAX          1024U

static const char* wmbus_log_path(WmBusCsvLogging logging) {
    return (logging == WmBusCsvLoggingFull) ? WMBUS_PACKET_LOG_FULL_PATH :
                                              WMBUS_PACKET_LOG_BASIC_PATH;
}

static const char* wmbus_log_header(WmBusCsvLogging logging) {
    return (logging == WmBusCsvLoggingFull) ?
               "tick,mode,status,plausible,crc_ok,mfg,id,version,device_type,ci,rssi,parser,"
               "summary_a,summary_b,security_mode,decrypted,key_index,fields,packet_hex\n" :
               "tick,mode,status,mfg,id,version,device_type,ci,rssi,parser,summary_a,summary_b\n";
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

static void wmbus_log_format_hex(
    const uint8_t* data,
    size_t data_len,
    char* out,
    size_t out_size) {
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

bool wmbus_log_append(
    Storage* storage,
    WmBusCsvLogging logging,
    const WmBusPacketRecord* record) {
    if(!storage || !record || logging == WmBusCsvLoggingNone) return false;

    if(!wmbus_storage_ensure_app_folder(storage)) {
        return false;
    }

    File* file = storage_file_alloc(storage);
    bool written = false;

    do {
        const char* path = wmbus_log_path(logging);
        if(!storage_file_open(file, path, FSAM_READ_WRITE, FSOM_OPEN_APPEND)) {
            break;
        }

        if(storage_file_size(file) == 0U) {
            if(!wmbus_log_write_line(file, "%s", wmbus_log_header(logging))) {
                break;
            }
        }

        char fields[384] = {0};
        char packet_hex[513] = {0};
        wmbus_packet_build_fields_text(record, fields, sizeof(fields));
        wmbus_log_format_hex(record->packet_bytes, record->packet_len, packet_hex, sizeof(packet_hex));

        if(logging == WmBusCsvLoggingFull) {
            written = wmbus_log_write_line(
                file,
                "%lu,%c,%s,%s,%s,%s,%s,%02X,%02X,%02X,%d,%s,%s,%s,%02X,%s,%u,%s,%s\n",
                (unsigned long)record->rx_tick,
                record->mode == WmBusRxModeT ? 'T' : 'C',
                wmbus_packet_status_str(record->status),
                record->plausible ? "yes" : "no",
                record->crc_known ? (record->crc_ok ? "yes" : "no") : "",
                record->packet_is_frame ? record->frame.mfg : "",
                record->packet_is_frame ? record->frame.id_str : "",
                record->packet_is_frame ? record->frame.version : 0U,
                record->packet_is_frame ? record->frame.dev_type : 0U,
                record->packet_is_frame ? record->frame.ci_field : 0U,
                record->rssi,
                record->application.parser_name,
                record->application.summary_a,
                record->application.summary_b,
                record->transport.has_short_tpl ? record->transport.security_mode : 0U,
                record->transport.decrypted ? "yes" : "no",
                record->transport.key_index,
                fields,
                packet_hex);
        } else {
            written = wmbus_log_write_line(
                file,
                "%lu,%c,%s,%s,%s,%02X,%02X,%02X,%d,%s,%s,%s\n",
                (unsigned long)record->rx_tick,
                record->mode == WmBusRxModeT ? 'T' : 'C',
                wmbus_packet_status_str(record->status),
                record->packet_is_frame ? record->frame.mfg : "",
                record->packet_is_frame ? record->frame.id_str : "",
                record->packet_is_frame ? record->frame.version : 0U,
                record->packet_is_frame ? record->frame.dev_type : 0U,
                record->packet_is_frame ? record->frame.ci_field : 0U,
                record->rssi,
                record->application.parser_name,
                record->application.summary_a,
                record->application.summary_b);
        }

        storage_file_sync(file);
    } while(false);

    if(storage_file_is_open(file)) {
        storage_file_close(file);
    }
    storage_file_free(file);
    return written;
}
