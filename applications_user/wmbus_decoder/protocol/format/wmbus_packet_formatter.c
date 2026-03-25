#include "wmbus_packet_formatter.h"

#include <stdio.h>
#include <string.h>

#include "wmbus_packet_summary.h"
#include "wmbus_record_formatter.h"
#include "../model/wmbus_application_record.h"
#include "../parser/wmbus_parser.h"

static void wmbus_packet_formatter_format_tpl_fields(
    const WmBusPacketTplData* tpl,
    char* out,
    size_t out_size) {
    if(!tpl || !out || out_size == 0U) return;
    out[0] = '\0';
    if(!tpl->has_short_tpl) return;

    char key_desc[24] = {0};
    size_t write = 0U;

    int len = snprintf(&out[write], out_size - write, "ACC=%02X", tpl->acc);
    if(len < 0 || (size_t)len >= (out_size - write)) return;
    write += (size_t)len;

    len = snprintf(&out[write], out_size - write, ";TPL=%02X", tpl->tpl_status);
    if(len < 0 || (size_t)len >= (out_size - write)) return;
    write += (size_t)len;

    len = snprintf(&out[write], out_size - write, ";CFG=%04X", tpl->cfg);
    if(len < 0 || (size_t)len >= (out_size - write)) return;
    write += (size_t)len;

    len = snprintf(&out[write], out_size - write, ";SEC=%02X", tpl->security_mode);
    if(len < 0 || (size_t)len >= (out_size - write)) return;
    write += (size_t)len;

    if(tpl->decrypted) {
        if(tpl->key_index != 0U) {
            snprintf(key_desc, sizeof(key_desc), "#%u", (unsigned int)tpl->key_index);
        } else {
            snprintf(key_desc, sizeof(key_desc), "zero");
        }
        snprintf(&out[write], out_size - write, ";Key=%s", key_desc);
    } else if(wmbus_parser_short_tpl_security_likely_encrypted(tpl->cfg)) {
        snprintf(&out[write], out_size - write, ";Payload=Encrypted");
    }
}

static void wmbus_packet_formatter_append_line(
    FuriString* out,
    const char* line,
    bool* wrote_any) {
    if(!out || !line || line[0] == '\0' || !wrote_any) {
        return;
    }

    if(*wrote_any) {
        furi_string_push_back(out, '\n');
    }
    furi_string_cat_str(out, line);
    *wrote_any = true;
}

static const WmBusApplicationRecord* wmbus_packet_formatter_find_first_record(
    const WmBusPacketApplicationData* application,
    WmBusApplicationQuantity quantity) {
    if(!application) {
        return NULL;
    }

    for(uint8_t i = 0U; i < application->record_count; i++) {
        const WmBusApplicationRecord* record = &application->records[i];
        if(record->quantity == quantity && wmbus_application_record_is_meaningful(record)) {
            return record;
        }
    }

    return NULL;
}

static void wmbus_packet_formatter_append_filtered_joined_fields(
    const FuriString* fields,
    bool skip_status,
    bool skip_volume,
    FuriString* out,
    bool* wrote_any) {
    if(!fields || !out || !wrote_any || furi_string_empty(fields)) {
        return;
    }

    const char* text = furi_string_get_cstr(fields);
    size_t text_len = furi_string_size(fields);
    size_t start = 0U;

    while(start < text_len) {
        size_t end = start;
        while(end < text_len && text[end] != '\n') {
            end++;
        }

        char line[96] = {0};
        size_t line_len = end - start;
        if(line_len >= sizeof(line)) {
            line_len = sizeof(line) - 1U;
        }
        memcpy(line, &text[start], line_len);
        line[line_len] = '\0';

        bool is_status = strncmp(line, "Status=", 7U) == 0;
        bool is_volume = strncmp(line, "Volume=", 7U) == 0;
        if((skip_status && is_status) || (skip_volume && is_volume)) {
            start = end + 1U;
            continue;
        }

        wmbus_packet_formatter_append_line(out, line, wrote_any);
        start = end + 1U;
    }
}

static void wmbus_packet_formatter_format_application_detail(
    const WmBusPacketRecord* record,
    FuriString* out) {
    FuriString* fields = furi_string_alloc();
    FuriString* status_field = furi_string_alloc();
    char tpl_fields[96] = {0};
    char line[96] = {0};
    char total[WMBUS_PACKET_VALUE_MAX] = {0};
    bool wrote_any = false;
    bool wrote_status = false;
    bool wrote_volume = false;

    if(!fields || !status_field) {
        furi_string_set(out, "-");
        if(fields) {
            furi_string_free(fields);
        }
        if(status_field) {
            furi_string_free(status_field);
        }
        return;
    }

    const WmBusApplicationRecord* status_record = wmbus_packet_formatter_find_first_record(
        &record->application, WmBusApplicationQuantityStatus);
    if(status_record && wmbus_record_formatter_format_field(status_record, status_field) &&
       !furi_string_empty(status_field)) {
        wmbus_packet_formatter_append_line(out, furi_string_get_cstr(status_field), &wrote_any);
        wrote_status = true;
    }

    uint32_t total_m3_x1000 = 0U;
    if(wmbus_packet_summary_find_total_m3(&record->application, &total_m3_x1000)) {
        wmbus_packet_summary_format_total_m3(total_m3_x1000, total, sizeof(total), true);
        snprintf(line, sizeof(line), "Volume=%s", total);
        wmbus_packet_formatter_append_line(out, line, &wrote_any);
        wrote_volume = true;
    }

    wmbus_record_formatter_format_joined(
        record->application.records, record->application.record_count, '\n', fields);
    wmbus_packet_formatter_append_filtered_joined_fields(
        fields, wrote_status, wrote_volume, out, &wrote_any);

    if(!wrote_any) {
        wmbus_packet_formatter_format_tpl_fields(&record->tpl, tpl_fields, sizeof(tpl_fields));
        if(tpl_fields[0] != '\0') {
            wmbus_packet_formatter_append_line(out, "TPL:", &wrote_any);
            furi_string_cat_str(out, " ");
            furi_string_cat_str(out, tpl_fields);
        }
    }

    if(!wrote_any) {
        furi_string_set(out, "-");
    }

    furi_string_free(fields);
    furi_string_free(status_field);
}

static void wmbus_packet_formatter_format_frame_detail(
    const WmBusPacketRecord* record,
    FuriString* out) {
    FuriString* application_body = furi_string_alloc();
    if(!application_body) {
        furi_string_set(out, "");
        return;
    }

    char security[48] = {0};
    wmbus_packet_summary_format_security_text(&record->tpl, security, sizeof(security));
    if(security[0] == '\0') {
        snprintf(security, sizeof(security), "-");
    }

    wmbus_packet_formatter_format_application_detail(record, application_body);
    furi_string_printf(
        out,
        "MFC: %s  DT: %02X  CI: %02X\nID: %s\nMode: %c  RSSI: %d\n---\nStatus: %s\nParser: %s\nSecurity: %s\n---\n",
        record->identity.manufacturer,
        record->dll.dev_type,
        record->dll.ci_field,
        record->identity.meter_id,
        record->mode == WmBusRxModeT ? 'T' : 'C',
        record->rssi,
        wmbus_packet_status_str(record->status),
        wmbus_parser_id_name(record->application.parser_id),
        security);
    furi_string_cat_str(out, furi_string_get_cstr(application_body));
    furi_string_free(application_body);
}

static void wmbus_packet_formatter_format_raw_detail(
    const WmBusPacketRecord* record,
    FuriString* out) {
    furi_string_printf(
        out,
        "MFC: -  DT: --  CI: --\nID: -\nMode: %c  RSSI: %d\n---\nStatus: %s\nParser: %s\nSecurity: -\n---\nLen=%u bytes",
        record->mode == WmBusRxModeT ? 'T' : 'C',
        record->rssi,
        wmbus_packet_status_str(record->status),
        wmbus_parser_id_name(record->application.parser_id),
        (unsigned int)record->packet_len);
}

void wmbus_packet_format_detail_text(const WmBusPacketRecord* record, FuriString* out) {
    if(!out) return;
    furi_string_reset(out);
    if(!record) return;

    if(record->packet_is_frame) {
        wmbus_packet_formatter_format_frame_detail(record, out);
    } else {
        wmbus_packet_formatter_format_raw_detail(record, out);
    }
}
