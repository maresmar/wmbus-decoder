#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../../core/wmbus_types.h"
#include "wmbus_parser_id.h"

typedef struct {
    WmBusParserId parser_id;
    const char* name;
    bool validates_decrypt;
} WmBusParserInfo;

const WmBusParserInfo* wmbus_parser_get_info(WmBusParserId parser_id);
const char* wmbus_parser_id_name(WmBusParserId parser_id);

bool wmbus_parser_ci_has_ell(uint8_t ci);
bool wmbus_parser_ell_has_session_fields(uint8_t ci);
uint8_t wmbus_parser_ell_security_mode(uint32_t sn);
bool wmbus_parser_ell_security_likely_encrypted(uint32_t sn);

uint8_t wmbus_parser_short_tpl_security_mode(uint16_t cfg);
bool wmbus_parser_short_tpl_security_likely_encrypted(uint16_t cfg);
