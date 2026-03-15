#include "wmbus_device_parser.h"

#include <stdio.h>

#include "wmbus_parser_apator162.h"
#include "wmbus_parser_dif_vif.h"

static const WmBusDeviceParser wmbus_device_parsers[] = {
    {
        .name = "Apator162",
        .probe = wmbus_parser_apator162_probe,
        .parse = wmbus_parser_apator162_parse,
    },
    {
        // Generic parser, should be last one
        .name = "DifVif",
        .probe = wmbus_parser_dif_vif_probe,
        .parse = wmbus_parser_dif_vif_parse,
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
            if(record->application.parser_name[0] == '\0') {
                snprintf(
                    record->application.parser_name,
                    sizeof(record->application.parser_name),
                    "%s",
                    parser->name);
            }
            return true;
        }
        if(i + 1U < COUNT_OF(wmbus_device_parsers)) {
            return false;
        }
    }

    return false;
}
