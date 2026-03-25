#include "wmbus_packet_parser.h"

#include <string.h>

#include "../parser/wmbus_device_parser.h"

bool wmbus_packet_parse_application(WmBusPacketRecord* record) {
    if(!record) {
        return false;
    }

    memset(&record->application, 0, sizeof(record->application));
    if(!record->packet_is_frame || !record->payload.has_application_payload ||
       record->payload.application_len == 0U) {
        return false;
    }

    WmBusParserPacketView packet = {
        .dll = &record->dll,
        .tpl = &record->tpl,
        .payload = &record->payload,
        .identity = &record->identity,
    };

    return wmbus_device_parser_apply(&packet, &record->application);
}

void wmbus_packet_finalize_parser(WmBusPacketRecord* record) {
    if(!record) return;

    if(record->application.parser_id == WmBusParserIdUnknown) {
        record->application.parser_id = record->tpl.has_short_tpl ? WmBusParserIdShortTpl :
                                                                     (record->packet_is_frame ?
                                                                          WmBusParserIdHeader :
                                                                          WmBusParserIdRaw);
    }
}
