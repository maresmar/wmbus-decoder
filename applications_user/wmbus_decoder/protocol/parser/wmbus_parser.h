#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../model/wmbus_application_types.h"
#include "../../core/wmbus_types.h"

typedef struct {
    WmBusParserId parser_id;
    const char* name;
    bool validates_decrypt;
    bool show_detail;
} WmBusParserInfo;

const WmBusParserInfo* wmbus_parser_get_info(WmBusParserId parser_id);
const char* wmbus_parser_id_name(WmBusParserId parser_id);
bool wmbus_parser_validates_decrypt(WmBusParserId parser_id);
bool wmbus_parser_show_detail(WmBusParserId parser_id);

uint8_t wmbus_parser_short_tpl_security_mode(uint16_t cfg);
bool wmbus_parser_short_tpl_security_likely_encrypted(uint16_t cfg);
bool wmbus_parser_short_tpl_payload_has_check_bytes(const uint8_t* frame, size_t frame_len);
