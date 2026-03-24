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
        // Generic parser, should be last one
        .parser_id = WmBusParserIdDifVif,
        .probe = wmbus_parser_dif_vif_probe,
        .parse = wmbus_parser_dif_vif_parse,
    },
};

bool wmbus_device_parser_apply(
    WmBusPacketRecord* record,
    const WmBusPacketParseContext* parse_context) {
    if(!record) {
        return false;
    }

    for(size_t i = 0; i < COUNT_OF(wmbus_device_parsers); i++) {
        const WmBusDeviceParser* parser = &wmbus_device_parsers[i];
        if(!parser->probe || !parser->parse) {
            continue;
        }
        if(!parser->probe(record, parse_context)) {
            continue;
        }
        if(parser->parse(record, parse_context)) {
            if(record->application.parser_id == WmBusParserIdUnknown) {
                record->application.parser_id = parser->parser_id;
            }
            return true;
        }
        if(i + 1U < COUNT_OF(wmbus_device_parsers)) {
            return false;
        }
    }

    return false;
}
