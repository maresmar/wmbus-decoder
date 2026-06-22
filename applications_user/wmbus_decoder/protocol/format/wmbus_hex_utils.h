#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * Encode byte array to hexadecimal string (big-endian).
 *
 * @param data Input byte array
 * @param data_len Length of input array
 * @param out Output buffer
 * @param out_size Size of output buffer
 */
void wmbus_hex_encode(const uint8_t* data, size_t data_len, char* out, size_t out_size);

/**
 * Encode 64-bit unsigned integer to hexadecimal string (little-endian).
 *
 * @param value Input value
 * @param data_len Number of bytes to encode (1-8)
 * @param out Output buffer
 * @param out_size Size of output buffer
 * @return true if successful, false if invalid parameters or buffer too small
 */
bool wmbus_hex_encode_le(uint64_t value, uint8_t data_len, char* out, size_t out_size);
