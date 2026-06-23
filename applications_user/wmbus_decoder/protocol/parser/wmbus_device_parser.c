#include "wmbus_device_parser.h"

#include "wmbus_parser_apator162.h"
#include "wmbus_parser_dif_vif.h"

/*
 * Parser order is semantic: the first parser whose probe+parse succeeds wins.
 *
 * Keep device-specific parsers before broader fallbacks such as DifVif, and
 * place any new specialized parser ahead of the more generic parser it should
 * override.
 */
static const WmBusDeviceParser wmbus_device_parsers[] = {
    {
        .info =
            {
                .parser_id = WmBusParserIdApator162,
                .name = "Apator162",
                .validates_decrypt = true,
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

static bool wmbus_device_parser_try(
    const WmBusParserPacketView* packet,
    bool validate_decrypt_only,
    WmBusParserId* out_parser_id,
    WmBusPacketApplicationData* out_application) {
    if(!packet) {
        return false;
    }

    for(size_t i = 0; i < (sizeof(wmbus_device_parsers) / sizeof(wmbus_device_parsers[0])); i++) {
        const WmBusDeviceParser* parser = &wmbus_device_parsers[i];
        if(!parser->probe || !parser->parse) {
            continue;
        }
        if(validate_decrypt_only && !parser->info.validates_decrypt) {
            continue;
        }
        if(!parser->probe(packet)) {
            continue;
        }

        WmBusPacketApplicationData application = {.parser_id = parser->info.parser_id};
        if(parser->parse(packet, &application)) {
            if(out_application) {
                *out_application = application;
            }
            if(out_parser_id) {
                *out_parser_id = parser->info.parser_id;
            }
            return true;
        }
    }

    return false;
}

bool wmbus_device_parser_apply(
    const WmBusParserPacketView* packet,
    WmBusPacketApplicationData* out_application) {
    if(!out_application) {
        return false;
    }

    return wmbus_device_parser_try(packet, false, NULL, out_application);
}

bool wmbus_device_parser_validate_decrypt(
    const WmBusParserPacketView* packet,
    WmBusParserId* out_parser_id) {
    return wmbus_device_parser_try(packet, true, out_parser_id, NULL);
}
