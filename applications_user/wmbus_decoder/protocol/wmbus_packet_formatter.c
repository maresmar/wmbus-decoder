#include "wmbus_packet_formatter.h"

#include <stdio.h>

#include "wmbus_application_record.h"
#include "wmbus_record_formatter.h"
#include "parser/wmbus_parser.h"

static const char* wmbus_packet_formatter_security_mode_name(uint8_t security_mode) {
    switch(security_mode) {
    case 0x00:
        return "Clear";
    case 0x01:
        return "Manufacturer";
    case 0x05:
        return "AES-CBC IV";
    case 0x08:
        return "AES-CTR CMAC";
    default:
        return NULL;
    }
}

static void wmbus_packet_formatter_format_total_m3(
    uint32_t total_m3_x1000,
    char* out,
    size_t out_size) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';

    uint32_t whole = total_m3_x1000 / 1000U;
    uint32_t frac = total_m3_x1000 % 1000U;
    snprintf(out, out_size, "%lu.%03lu m3", (unsigned long)whole, (unsigned long)frac);
}

static void wmbus_packet_formatter_format_security_text(
    const WmBusPacketTplData* tpl,
    char* out,
    size_t out_size) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';
    if(!tpl || !tpl->has_short_tpl) return;

    char mode[20] = {0};
    const char* known_mode = wmbus_packet_formatter_security_mode_name(tpl->security_mode);
    if(known_mode) {
        snprintf(mode, sizeof(mode), "%s", known_mode);
    } else {
        snprintf(mode, sizeof(mode), "Mode %02X", tpl->security_mode);
    }

    if(tpl->decrypted) {
        if(tpl->key_index != 0U) {
            snprintf(out, out_size, "%s, decrypted key #%u", mode, (unsigned int)tpl->key_index);
        } else {
            snprintf(out, out_size, "%s, decrypted zero key", mode);
        }
    } else if(wmbus_parser_short_tpl_security_likely_encrypted(tpl->cfg)) {
        snprintf(out, out_size, "%s, encrypted", mode);
    } else {
        snprintf(out, out_size, "%s", mode);
    }
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

    if(wmbus_application_find_total_volume(
           record->application.records,
           record->application.record_count,
           &total_m3_x1000)) {
        wmbus_packet_formatter_format_total_m3(total_m3_x1000, total, sizeof(total));
    }
    wmbus_packet_formatter_format_security_text(&record->tpl, security, sizeof(security));
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
        record->dll.mfg,
        record->dll.dev_type,
        record->dll.id_str,
        record->mode == WmBusRxModeT ? 'T' : 'C',
        record->rssi,
        record->dll.ci_field,
        record->dll.version);

    if(!wmbus_parser_id_is_generic(record->application.parser_id)) {
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
