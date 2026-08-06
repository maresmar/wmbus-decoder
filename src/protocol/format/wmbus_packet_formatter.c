#include "wmbus_packet_formatter.h"

#include <stdio.h>

#include "wmbus_packet_summary.h"
#include "wmbus_record_formatter.h"
#include "../model/wmbus_application_record.h"
#include "../parser/wmbus_parser.h"

static void wmbus_packet_formatter_format_application_detail(
    const WmBusPacketRecord* record,
    FuriString* out) {
    FuriString* fields = furi_string_alloc();

    if(!fields) {
        furi_string_set(out, "-");
        return;
    }

    wmbus_record_formatter_format_joined(
        record->application.records, record->application.record_count, '\n', fields);
    if(furi_string_empty(fields)) {
        furi_string_set(out, "-");
    } else {
        furi_string_set(out, furi_string_get_cstr(fields));
    }

    furi_string_free(fields);
}

static void
    wmbus_packet_formatter_format_frame_detail(const WmBusPacketRecord* record, FuriString* out) {
    FuriString* application_body = furi_string_alloc();
    if(!application_body) {
        furi_string_set(out, "");
        return;
    }

    char security[48] = {0};
    wmbus_packet_summary_format_security_text(
        &record->ell, &record->tpl, security, sizeof(security));
    if(security[0] == '\0') {
        snprintf(security, sizeof(security), "-");
    }

    wmbus_packet_formatter_format_application_detail(record, application_body);
    furi_string_printf(
        out,
        "Manufacturer: %s\nDevice type: %02X\nCI field: %02X\nMeter ID: %s\nMode: %c frame: %s\nRSSI: %d dBm\n---\nQuality: %s\nParser: %s\nSecurity: %s\n---\n",
        record->identity.manufacturer,
        record->dll.dev_type,
        record->dll.ci_field,
        record->identity.meter_id,
        record->mode == WmBusRxModeT ? 'T' : 'C',
        wmbus_packet_summary_frame_type(record->format),
        record->rssi,
        wmbus_packet_quality_str(record->quality),
        wmbus_parser_id_name(record->application.parser_id),
        security);
    furi_string_cat_str(out, furi_string_get_cstr(application_body));
    furi_string_free(application_body);
}

static void
    wmbus_packet_formatter_format_raw_detail(const WmBusPacketRecord* record, FuriString* out) {
    furi_string_printf(
        out,
        "Manufacturer: -\nDevice type: --\nCI field: --\nMeter ID: -\nRadio mode: %c, frame %s\nRSSI: %d dBm\n---\nQuality: %s\nParser: %s\nSecurity: -\n---\nPacket length=%u bytes",
        record->mode == WmBusRxModeT ? 'T' : 'C',
        wmbus_packet_summary_frame_type(record->format),
        record->rssi,
        wmbus_packet_quality_str(record->quality),
        wmbus_parser_id_name(record->application.parser_id),
        (unsigned int)record->packet_len);
}

void wmbus_packet_format_detail_text(const WmBusPacketRecord* record, FuriString* out) {
    if(!out) return;
    furi_string_reset(out);
    if(!record) return;

    if(wmbus_packet_quality_meets(record->quality, WmBusPacketQualityFrameComplete)) {
        wmbus_packet_formatter_format_frame_detail(record, out);
    } else {
        wmbus_packet_formatter_format_raw_detail(record, out);
    }
}

void wmbus_packet_format_application_text(const WmBusPacketRecord* record, FuriString* out) {
    if(!out) return;
    furi_string_reset(out);
    if(!record) return;

    if(!wmbus_packet_quality_meets(record->quality, WmBusPacketQualityFrameComplete)) {
        furi_string_set(out, "No complete frame.");
        return;
    }

    wmbus_packet_formatter_format_application_detail(record, out);
    if(furi_string_equal_str(out, "-")) {
        furi_string_set(out, "No application records.");
    }
}
