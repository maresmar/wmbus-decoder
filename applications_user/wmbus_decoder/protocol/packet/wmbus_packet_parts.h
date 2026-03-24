#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../frame/wmbus_frame.h"

typedef struct {
    uint8_t l_field;
    uint8_t c_field;
    uint16_t m_field;
    uint8_t id[4];
    uint8_t version;
    uint8_t dev_type;
    uint8_t ci_field;
} WmBusPacketDllData;

typedef struct {
    bool has_short_tpl;
    uint8_t header_len;
    uint8_t acc;
    uint8_t tpl_status;
    uint16_t cfg;
    uint8_t security_mode;
    bool decrypted;
    uint8_t key_index;
} WmBusPacketTplData;

typedef struct {
    uint16_t packet_offset;
    uint16_t packet_len;
} WmBusPacketPayloadData;
