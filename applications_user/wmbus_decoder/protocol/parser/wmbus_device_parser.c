#include "wmbus_device_parser.h"

#include "wmbus_parser_apator162.h"
#include "wmbus_parser_dif_vif.h"

static const WmBusDeviceParser wmbus_device_parsers[] = {
    {
        .info =
            {
                .parser_id = WmBusParserIdApator162,
                .name = "Apator162",
                .validates_decrypt = true,
                .show_detail = true,
            },
        .probe = wmbus_parser_apator162_probe,
        .parse = wmbus_parser_apator162_parse,
    },
    {
        .info =
            {
                .parser_id = WmBusParserIdDifVif,
                .name = "DifVif",
                .validates_decrypt = false,
                .show_detail = false,
            },
        .probe = wmbus_parser_dif_vif_probe,
        .parse = wmbus_parser_dif_vif_parse,
    },
};

const WmBusDeviceParser* wmbus_device_parser_get(WmBusParserId parser_id) {
    for(size_t i = 0; i < (sizeof(wmbus_device_parsers) / sizeof(wmbus_device_parsers[0])); i++) {
        if(wmbus_device_parsers[i].info.parser_id == parser_id) {
            return &wmbus_device_parsers[i];
        }
    }

    return NULL;
}

bool wmbus_device_parser_apply(
    const WmBusParserPacketView* packet,
    const WmBusPacketParseContext* parse_context,
    WmBusPacketApplicationData* out_application) {
    if(!packet || !out_application) {
        return false;
    }

    for(size_t i = 0; i < (sizeof(wmbus_device_parsers) / sizeof(wmbus_device_parsers[0])); i++) {
        const WmBusDeviceParser* parser = &wmbus_device_parsers[i];
        if(!parser->probe || !parser->parse) {
            continue;
        }
        if(!parser->probe(packet, parse_context)) {
            continue;
        }
        WmBusPacketApplicationData application = {.parser_id = parser->info.parser_id};
        if(parser->parse(packet, parse_context, &application)) {
            *out_application = application;
            return true;
        }
    }

    return false;
}
