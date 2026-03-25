#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../parser/wmbus_parser_id.h"

#define WMBUS_PACKET_RECORD_MAX 12U

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
    WmBusApplicationQuantityUnknown = 0,
    WmBusApplicationQuantityVolume,
    WmBusApplicationQuantityEnergy,
    WmBusApplicationQuantityPower,
    WmBusApplicationQuantityVolumeFlow,
    WmBusApplicationQuantityFlowTemperature,
    WmBusApplicationQuantityReturnTemperature,
    WmBusApplicationQuantityTemperatureDifference,
    WmBusApplicationQuantityDate,
    WmBusApplicationQuantityDateTime,
    WmBusApplicationQuantityStatus,
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
    WmBusParserId parser_id;
    uint8_t record_count;
    WmBusApplicationRecord records[WMBUS_PACKET_RECORD_MAX];
} WmBusPacketApplicationData;
