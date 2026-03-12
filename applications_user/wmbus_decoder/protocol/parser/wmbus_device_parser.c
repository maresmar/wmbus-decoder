#include "wmbus_device_parser.h"

#include "wmbus_parser_apator162.h"

static const WmBusDeviceParser wmbus_device_parsers[] = {
    {
        .name = "Apator162",
        .probe = wmbus_parser_apator162_probe,
        .parse = wmbus_parser_apator162_parse,
    },
};

bool wmbus_device_parser_apply(WmBusPacketRecord* record) {
    if(!record) {
        return false;
    }

    for(size_t i = 0; i < COUNT_OF(wmbus_device_parsers); i++) {
        const WmBusDeviceParser* parser = &wmbus_device_parsers[i];
        if(!parser->probe || !parser->parse) {
            continue;
        }
        if(!parser->probe(record)) {
            continue;
        }
        if(parser->parse(record)) {
            return true;
        }
    }

    return false;
}
