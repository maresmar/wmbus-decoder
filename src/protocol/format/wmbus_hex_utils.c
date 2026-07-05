#include "wmbus_hex_utils.h"

#include <stdio.h>

void wmbus_hex_encode(const uint8_t* data, size_t data_len, char* out, size_t out_size) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';

    if(!data) return;

    size_t write = 0;
    for(size_t i = 0; i < data_len && (write + 2U) < out_size; i++) {
        snprintf(&out[write], out_size - write, "%02X", data[i]);
        write += 2U;
    }
    out[write] = '\0';
}

bool wmbus_hex_encode_le(uint64_t value, uint8_t data_len, char* out, size_t out_size) {
    if(!out || out_size == 0U || data_len == 0U || data_len > 8U) return false;
    out[0] = '\0';

    size_t write = 0U;
    for(uint8_t i = 0; i < data_len; i++) {
        int len = snprintf(
            &out[write], out_size - write, "%02X", (unsigned int)((value >> (8U * i)) & 0xFFU));
        if(len < 0 || (size_t)len >= (out_size - write)) {
            out[0] = '\0';
            return false;
        }
        write += (size_t)len;
    }

    return true;
}
