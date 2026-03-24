#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "capture/wmbus_capture.h"
#include "frame/wmbus_frame.h"
#include "../core/wmbus_types.h"
#include "../storage/wmbus_keyring.h"

#define WMBUS_PACKET_PAYLOAD_MAX     256U
#define WMBUS_PACKET_RECORD_MAX      12U
#define WMBUS_PACKET_LABEL_MAX       32U
#define WMBUS_PACKET_VALUE_MAX       32U
#define WMBUS_PACKET_PARSER_NAME_MAX 16U

typedef enum {
    WmBusApplicationValueNone = 0,
    WmBusApplicationValueUnsigned,
    WmBusApplicationValueDateTime,
    WmBusApplicationValueRaw,
} WmBusApplicationValueType;

typedef enum {
    WmBusApplicationMeasurementTypeUnknown = 0,
    WmBusApplicationMeasurementTypeInstantaneous,
    WmBusApplicationMeasurementTypeMinimum,
    WmBusApplicationMeasurementTypeMaximum,
    WmBusApplicationMeasurementTypeAtError,
} WmBusApplicationMeasurementType;

typedef enum {
    WmBusApplicationQuantityUnknown =
        0, /**< No semantic decode; formatter may render raw bytes. */
    WmBusApplicationQuantityVolume, /**< Unsigned cubic meters, scaled by `scale10`. */
    WmBusApplicationQuantityEnergy, /**< Unsigned watt-hours, scaled by `scale10`. */
    WmBusApplicationQuantityPower, /**< Unsigned watts, scaled by `scale10`. */
    WmBusApplicationQuantityVolumeFlow, /**< Unsigned cubic meters per hour, scaled by `scale10`. */
    WmBusApplicationQuantityFlowTemperature, /**< Unsigned degrees C, scaled by `scale10`. */
    WmBusApplicationQuantityReturnTemperature, /**< Unsigned degrees C, scaled by `scale10`. */
    WmBusApplicationQuantityTemperatureDifference, /**< Unsigned kelvin, scaled by `scale10`. */
    WmBusApplicationQuantityDate, /**< Structured date in `value_datetime`, `has_time = false`. */
    WmBusApplicationQuantityDateTime, /**< Structured date-time in `value_datetime`, `has_time = true`. */
    WmBusApplicationQuantityStatus, /**< Raw status payload; formatter normally renders hex bytes. */
} WmBusApplicationQuantity;

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    bool has_time;
} WmBusApplicationDateTime;

typedef struct {
    uint8_t dif;
    uint8_t vif;
    uint16_t storage_no;
    uint8_t tariff;
    uint8_t subunit;
    uint8_t data_len;
    WmBusApplicationValueType value_type;
    WmBusApplicationMeasurementType measurement_type;
    WmBusApplicationQuantity quantity;
    int8_t scale10;
    uint64_t value_unsigned;
    WmBusApplicationDateTime value_datetime;
} WmBusApplicationRecord;

typedef struct {
    uint8_t l_field;
    uint8_t c_field;
    uint16_t m_field;
    uint8_t id[4];
    char mfg[WMBUS_MFG_STR_LEN]; /**< Derived from `m_field`. */
    char id_str[WMBUS_ID_STR_LEN]; /**< Derived from `id`. */
    bool id_is_bcd; /**< Derived while formatting `id`. */
    uint8_t version;
    uint8_t dev_type;
    uint8_t ci_field;
} WmBusPacketDllData;

typedef struct {
    bool has_short_tpl; /**< Helper derived from `dll.ci_field`. */
    uint8_t header_len; /**< Helper derived from `dll.ci_field` and short-TPL presence. */
    uint8_t acc;
    uint8_t tpl_status;
    uint16_t cfg;
    uint8_t security_mode; /**< Helper derived from `cfg`. */
    bool decrypted;
    uint8_t key_index;
} WmBusPacketTplData;

typedef struct {
    uint16_t packet_offset; /**< Slice into `packet_bytes` after DLL/TPL header parsing. */
    uint16_t packet_len; /**< Length of payload bytes available at `packet_bytes + packet_offset`. */
    bool has_application_payload; /**< Derived from payload slicing and decrypt result. */
    uint16_t application_len;
    uint8_t application_payload[WMBUS_PACKET_PAYLOAD_MAX];
} WmBusPacketPayloadData;

typedef struct {
    char parser_name[WMBUS_PACKET_PARSER_NAME_MAX];
    uint8_t record_count;
    WmBusApplicationRecord records[WMBUS_PACKET_RECORD_MAX];
} WmBusPacketApplicationData;

typedef struct {
    WmBusStatus status;
    WmBusRxMode mode;
    bool decoded_ok;
    bool plausible;
    bool length_ok;
    bool crc_known;
    bool crc_ok;
    bool strong_rssi;
    uint16_t raw_len;
    uint16_t
        packet_len; /**< Stored byte count of `packet_bytes`, not the same as `dll.l_field`. */
    bool packet_is_frame;
    int best_offset;
    int rssi;
    uint32_t rx_tick;
    uint8_t packet_bytes[256];
    WmBusPacketDllData dll;
    WmBusPacketTplData tpl;
    WmBusPacketPayloadData payload;
    WmBusPacketApplicationData application;
} WmBusPacketRecord;

bool wmbus_packet_process_capture(
    const WmBusCaptureFrame* capture,
    const WmBusKeyring* keyring,
    WmBusPacketRecord* record);

const char* wmbus_packet_status_str(WmBusStatus status);
const char* wmbus_packet_status_short_label(WmBusStatus status);
const char* wmbus_packet_csv_logging_str(WmBusCsvLogging logging);
void wmbus_packet_format_total_m3(uint32_t total_m3_x1000, char* out, size_t out_size);
void wmbus_packet_format_security_summary(
    bool has_short_tpl,
    uint8_t security_mode,
    bool security_likely_encrypted,
    bool decrypted,
    uint8_t key_index,
    char* out,
    size_t out_size);
void wmbus_packet_format_security_text(
    bool has_short_tpl,
    uint8_t security_mode,
    bool security_likely_encrypted,
    bool decrypted,
    uint8_t key_index,
    char* out,
    size_t out_size);
