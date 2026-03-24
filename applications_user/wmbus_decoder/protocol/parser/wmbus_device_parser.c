#include "wmbus_device_parser.h"

#include "wmbus_parser_apator162.h"
#include "wmbus_parser_dif_vif.h"

static const WmBusDeviceParser wmbus_device_parsers[] = {
    {
        .parser_id = WmBusParserIdApator162,
        .probe = wmbus_parser_apator162_probe,
        .parse = wmbus_parser_apator162_parse,
    },
    {
        .parser_id = WmBusParserIdDifVif,
        .probe = wmbus_parser_dif_vif_probe,
        .parse = wmbus_parser_dif_vif_parse,
    },
};

bool wmbus_device_parser_apply(
    const WmBusParserPacketView* packet,
    const WmBusPacketParseContext* parse_context,
    WmBusPacketApplicationData* out_application) {
    if(!packet || !out_application) {
        return false;
    }

    for(size_t i = 0; i < COUNT_OF(wmbus_device_parsers); i++) {
        const WmBusDeviceParser* parser = &wmbus_device_parsers[i];
        if(!parser->probe || !parser->parse) {
            continue;
        }
        if(!parser->probe(packet, parse_context)) {
            continue;
        }
        WmBusPacketApplicationData application = {.parser_id = parser->parser_id};
        if(parser->parse(packet, parse_context, &application)) {
            *out_application = application;
            return true;
        }
    }

    return false;
}
