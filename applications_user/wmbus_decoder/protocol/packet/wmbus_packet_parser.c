#include "wmbus_packet_parser.h"

#include <string.h>

#include "../parser/wmbus_device_parser.h"

bool wmbus_packet_parse_application(WmBusPacketRecord* record) {
    if(!record) {
        return false;
    }

    memset(&record->application, 0, sizeof(record->application));
    if(!wmbus_packet_quality_meets(record->quality, WmBusPacketQualityFrameComplete) ||
       !record->payload.has_application_payload || record->payload.application_len == 0U) {
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
        if(record->tpl.has_short_tpl) {
            record->application.parser_id = WmBusParserIdShortTpl;
        } else if(record->ell.has_ell) {
            record->application.parser_id = WmBusParserIdEll;
        } else {
            record->application.parser_id =
                wmbus_packet_quality_meets(record->quality, WmBusPacketQualityFrameComplete) ?
                    WmBusParserIdHeader :
                    WmBusParserIdRaw;
        }
    }
}
