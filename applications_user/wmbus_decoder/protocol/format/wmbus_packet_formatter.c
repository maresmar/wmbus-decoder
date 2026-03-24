#include "wmbus_packet_formatter.h"

#include <stdio.h>

#include "wmbus_packet_summary.h"
#include "wmbus_record_formatter.h"
#include "../parser/wmbus_parser.h"

static bool wmbus_packet_formatter_show_parser(WmBusParserId parser_id) {
    return parser_id != WmBusParserIdUnknown && parser_id != WmBusParserIdRaw &&
           parser_id != WmBusParserIdHeader && parser_id != WmBusParserIdShortTpl &&
           parser_id != WmBusParserIdDifVif;
}

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

static void wmbus_packet_formatter_format_detail_body(
    const WmBusPacketRecord* record,
    FuriString* out) {
    FuriString* fields = furi_string_alloc();
    uint32_t total_m3_x1000 = 0U;
    char total[WMBUS_PACKET_VALUE_MAX] = {0};
    char security[48] = {0};
    char tpl_fields[96] = {0};

    if(!fields) {
        furi_string_set(out, "-");
        return;
    }

    if(wmbus_packet_summary_find_total_m3(&record->application, &total_m3_x1000)) {
        wmbus_packet_summary_format_total_m3(total_m3_x1000, total, sizeof(total), true);
    }
    wmbus_packet_summary_format_security_text(&record->tpl, security, sizeof(security));
    wmbus_record_formatter_format_joined(
        record->application.records, record->application.record_count, '\n', fields);

    if(total[0] != '\0') {
        furi_string_cat_printf(out, "Total: %s", total);
        if(security[0] != '\0' || !furi_string_empty(fields)) {
            furi_string_push_back(out, '\n');
        }
    }

    if(security[0] != '\0') {
        furi_string_cat_printf(out, "Security: %s", security);
        if(!furi_string_empty(fields)) {
            furi_string_push_back(out, '\n');
        }
    }

    if(!furi_string_empty(fields)) {
        furi_string_cat_printf(out, "Fields:\n%s", furi_string_get_cstr(fields));
    } else {
        wmbus_packet_formatter_format_tpl_fields(&record->tpl, tpl_fields, sizeof(tpl_fields));
        if(tpl_fields[0] != '\0') {
            furi_string_cat_printf(out, "TPL: %s", tpl_fields);
        }
    }

    if(furi_string_empty(out)) {
        furi_string_set(out, "-");
    }

    furi_string_free(fields);
}

static void wmbus_packet_formatter_format_frame_detail(
    const WmBusPacketRecord* record,
    FuriString* out) {
    FuriString* detail_body = furi_string_alloc();
    if(!detail_body) {
        furi_string_set(out, "");
        return;
    }

    wmbus_packet_formatter_format_detail_body(record, detail_body);
    furi_string_printf(
        out,
        "Status: %s\nMF:%s  DT:%02X  ID:%s\nM:%c  R:%d\nCI:%02X  V:%02X\n",
        wmbus_packet_status_str(record->status),
        record->identity.manufacturer,
        record->dll.dev_type,
        record->identity.meter_id,
        record->mode == WmBusRxModeT ? 'T' : 'C',
        record->rssi,
        record->dll.ci_field,
        record->dll.version);

    if(wmbus_packet_formatter_show_parser(record->application.parser_id)) {
        furi_string_cat_printf(
            out, "Parser: %s\n", wmbus_parser_id_name(record->application.parser_id));
    }
    furi_string_cat_str(out, furi_string_get_cstr(detail_body));
    furi_string_free(detail_body);
}

static void wmbus_packet_formatter_format_raw_detail(
    const WmBusPacketRecord* record,
    FuriString* out) {
    furi_string_printf(
        out,
        "Status: %s\nMode: %c  RSSI: %d\nParser: %s\nLen: %u bytes",
        wmbus_packet_status_str(record->status),
        record->mode == WmBusRxModeT ? 'T' : 'C',
        record->rssi,
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
