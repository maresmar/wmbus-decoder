#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../protocol/frame/wmbus_frame.h"

#define WMBUS_SELFTEST_BUF_MAX 256U
#define WMBUS_BIT_NONE         SIZE_MAX
#define WMBUS_BYTE_LAST        (SIZE_MAX - 1U)

typedef struct {
    const char* name;
    const uint8_t* data;
    size_t len;
    bool is_t_raw;
    bool expect_plausible;
    bool expect_crc_ok;
    uint8_t expected_offset;
} WmBusTestVector;

typedef struct {
    const char* name;
    const WmBusTestVector* vector;
    bool build_format_a;
    size_t seed_corrupt_byte_pos;
    size_t frame_corrupt_byte_pos;
    size_t raw_corrupt_bit_pos;
} WmBusSelftestCase;

typedef struct {
    bool decoded_ok;
    bool plausible;
    bool length_ok;
    bool crc_ok;
    bool has_l_field;
    bool has_computed_len;
    bool has_identity;
    uint8_t l_field;
    size_t computed_len;
    int best_offset;
    char manufacturer[WMBUS_MFG_STR_LEN];
    char id[WMBUS_ID_STR_LEN];
} WmBusSelftestResult;

typedef struct {
    uint32_t total;
    uint32_t passed;
    uint32_t failed;
} WmBusSelftestSummary;

size_t wmbus_selftest_get_case_count(void);
const WmBusSelftestCase* wmbus_selftest_get_case(size_t index);

bool wmbus_selftest_run_case(const WmBusSelftestCase* test_case, WmBusSelftestResult* result);
void wmbus_selftest_log_case_result(
    const WmBusSelftestCase* test_case,
    const WmBusSelftestResult* result,
    bool pass);
void wmbus_selftest_run_all(WmBusSelftestSummary* summary, bool log_results);

void wmbus_run_selftests(void);
