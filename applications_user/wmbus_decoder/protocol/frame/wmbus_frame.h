#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../../core/wmbus_types.h"

#define WMBUS_MFG_STR_LEN 4U
#define WMBUS_ID_STR_LEN  9U

typedef struct {
    bool length_ok;
    bool crc_known;
    bool crc_ok;
    size_t computed_len;
    size_t normalized_len;
    WmBusFrameFormat format;
} WmBusFrameNormalizeResult;

typedef struct {
    bool complete;
    WmBusFrameFormat format;
    /** L-field-derived on-air frame byte count, including CRC bytes. */
    size_t frame_len;
    /** Frame length after removing link-layer CRC bytes. */
    size_t normalized_len;
} WmBusFrameMeasureResult;

void wmbus_frame_decode_mfg(uint16_t man, char out[WMBUS_MFG_STR_LEN]);
bool wmbus_frame_format_id_bcd(const uint8_t id[4], char out[WMBUS_ID_STR_LEN]);
void wmbus_frame_format_id(const uint8_t id[4], char out[WMBUS_ID_STR_LEN], bool* is_bcd);

bool wmbus_frame_l_field_valid(uint8_t l_field);
size_t wmbus_frame_len_format_a(uint8_t l_field);
size_t wmbus_frame_len_format_b(uint8_t l_field);
size_t wmbus_frame_expected_len(uint8_t l_field, WmBusFrameFormat format);

bool wmbus_frame_build_format_a(
    const uint8_t* normalized,
    size_t normalized_len,
    uint8_t* out,
    size_t out_max,
    size_t* out_len);

bool wmbus_frame_build_format_b(
    const uint8_t* normalized,
    size_t normalized_len,
    uint8_t* out,
    size_t out_max,
    size_t* out_len);

bool wmbus_frame_trim_crc(
    WmBusFrameFormat format,
    const uint8_t* data,
    size_t len,
    uint8_t* out,
    size_t out_max,
    size_t* out_len);

bool wmbus_frame_crc_check(WmBusFrameFormat format, const uint8_t* data, size_t len);

bool wmbus_frame_measure(
    WmBusRxMode mode,
    const uint8_t* frame,
    size_t frame_len,
    WmBusFrameMeasureResult* out);

bool wmbus_frame_normalize(
    WmBusRxMode mode,
    const uint8_t* frame,
    size_t frame_len,
    uint8_t* normalized,
    size_t normalized_max,
    WmBusFrameNormalizeResult* out);
