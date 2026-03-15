#pragma once

#include "wmbus_selftest.h"

#include "../protocol/capture/wmbus_capture.h"
#include "../protocol/frame/wmbus_frame.h"
#include "../protocol/wmbus_packet.h"
#include "../storage/wmbus_keyring.h"

#include <furi.h>
#include <storage/storage.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TAG                     "WmBusSelftest"
#define WMBUS_SELFTEST_BUF_MAX  256U
#define WMBUS_SELFTEST_LINE_MAX 256U
#define WMBUS_BIT_NONE          SIZE_MAX
#define WMBUS_BYTE_LAST         (SIZE_MAX - 1U)
#define WMBUS_APATOR_A_LEN      111U
#define WMBUS_APATOR_B_LEN      79U
#define WMBUS_APATOR_C_LEN      63U
#define WMBUS_SELFTEST_APATOR_PUBLIC_VECTOR_COUNT 14U

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

typedef bool (*WmBusSelftestCheckFn)(char* detail, size_t detail_len);

typedef struct {
    const char* name;
    WmBusSelftestCheckFn run;
} WmBusSelftestCheck;

typedef struct {
    const char* id;
    uint32_t total_m3_x1000;
    size_t parsed_len;
    uint32_t parsed_fnv1a;
    const char* telegram;
} WmBusSelftestApatorPublicVector;

typedef struct {
    const char* telegram;
    uint32_t total_m3_x1000;
    const char* id;
} WmBusSelftestApatorFieldVector;

extern const uint8_t wmbus_apator_a[];
extern const uint8_t wmbus_apator_b[];
extern const uint8_t wmbus_apator_c[];
extern const char* wmbus_selftest_apator_old_style_b6;
extern const size_t wmbus_selftest_apator_old_style_b6_len;
extern const uint32_t wmbus_selftest_apator_old_style_b6_fnv1a;
extern const char* wmbus_selftest_apator_encrypted_mode5;
extern const char* wmbus_selftest_apator_encrypted_mode5_gold;
extern const char* wmbus_selftest_apator_encrypted_mode5_field_02991035;
extern const char* wmbus_selftest_apator_encrypted_mode5_corrupt;
extern const WmBusSelftestApatorPublicVector wmbus_selftest_apator_public_vectors[];
extern const uint8_t wmbus_3of6_encode_lut[16];

void wmbus_selftest_set_detail(char* detail, size_t detail_len, const char* format, ...);
bool wmbus_selftest_hex_to_bytes(const char* hex, uint8_t* out, size_t out_max, size_t* out_len);
bool wmbus_selftest_hex_to_format_b_frame(
    const char* hex,
    uint8_t* out,
    size_t out_max,
    size_t* out_len);
uint32_t wmbus_selftest_fnv1a32(const uint8_t* data, size_t len);
bool wmbus_selftest_write_report_line(File* file, const char* format, ...);
bool wmbus_selftest_find_total_volume(const WmBusPacketRecord* record, uint32_t* total_m3_x1000);
const char* wmbus_selftest_record_value(
    const WmBusApplicationRecord* record,
    char* out,
    size_t out_size);
void wmbus_selftest_describe_first_record(
    const WmBusPacketRecord* packet,
    char* out,
    size_t out_size);
void wmbus_selftest_result_reset(WmBusSelftestResult* result);
bool wmbus_selftest_prepare_frame(
    const WmBusSelftestCase* test_case,
    const uint8_t** frame,
    size_t* frame_len);
bool wmbus_selftest_generate_t_3of6_raw_with_offset(
    const uint8_t* decoded,
    size_t decoded_len,
    uint8_t expected_offset,
    uint8_t* out,
    size_t out_max,
    size_t* out_len,
    size_t* out_bit_len);
void wmbus_selftest_corrupt_t_raw_bit(uint8_t* raw, size_t raw_bit_len, size_t bit_pos);
void wmbus_selftest_result_from_record(
    const WmBusPacketRecord* record,
    WmBusSelftestResult* result);
bool wmbus_selftest_process_capture_record(
    WmBusRxMode mode,
    const uint8_t* data,
    size_t data_len,
    const WmBusKeyring* keyring,
    WmBusPacketRecord* record);
bool wmbus_selftest_run_capture(
    WmBusRxMode mode,
    const uint8_t* data,
    size_t data_len,
    const WmBusKeyring* keyring,
    WmBusSelftestResult* result);
const char* wmbus_selftest_format_l_field(const WmBusSelftestResult* result, char out[8]);
const char* wmbus_selftest_format_computed_len(const WmBusSelftestResult* result, char out[16]);

size_t wmbus_selftest_get_case_count(void);
const WmBusSelftestCase* wmbus_selftest_get_case(size_t index);
bool wmbus_selftest_run_case(const WmBusSelftestCase* test_case, WmBusSelftestResult* result);
void wmbus_selftest_log_case_result(
    const WmBusSelftestCase* test_case,
    const WmBusSelftestResult* result,
    bool pass);
void wmbus_selftest_report_case_result(
    File* file,
    const WmBusSelftestCase* test_case,
    const WmBusSelftestResult* result,
    bool pass);

const WmBusSelftestCheck* wmbus_selftest_tooling_checks(size_t* count);
const WmBusSelftestCheck* wmbus_selftest_mode_checks(size_t* count);
const WmBusSelftestCheck* wmbus_selftest_parser_checks(size_t* count);
