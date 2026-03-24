#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../model/wmbus_application_types.h"
#include "../../core/wmbus_types.h"

const char* wmbus_parser_id_name(WmBusParserId parser_id);

uint8_t wmbus_parser_short_tpl_security_mode(uint16_t cfg);
bool wmbus_parser_short_tpl_security_likely_encrypted(uint16_t cfg);
bool wmbus_parser_short_tpl_payload_has_check_bytes(const uint8_t* frame, size_t frame_len);
WmBusMode5DecryptInfo wmbus_parser_decrypt_mode5(
    const uint8_t* frame,
    size_t frame_len,
    uint16_t cfg,
    const uint8_t key[16],
    uint8_t* out_frame);
