#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    WmBusRxModeT = 0,
    WmBusRxModeC = 1,
} WmBusRxMode;

typedef enum {
    WmBusStatusNone = 0,
    WmBusStatusDecodeFail,
    WmBusStatusNotPlausible,
    WmBusStatusFramingError,
    WmBusStatusCrcBad,
    WmBusStatusWeakRssi,
    WmBusStatusOk,
    WmBusStatusCount,
} WmBusStatus;

typedef enum {
    WmBusDecryptResultOk = 0,
    WmBusDecryptResultInvalidArgs,
    WmBusDecryptResultFrameTooShort,
    WmBusDecryptResultEncryptedPayloadTooShort,
} WmBusDecryptResult;

typedef struct {
    WmBusDecryptResult result;
    bool has_check_bytes;
} WmBusMode5DecryptInfo;

typedef enum {
    WmBusCsvLoggingNone = 0,
    WmBusCsvLoggingBasic,
    WmBusCsvLoggingFull,
    WmBusCsvLoggingCount,
} WmBusCsvLogging;

typedef uint32_t WmBusStatusMask;

#define WMBUS_STATUS_MASK(status) (1UL << (uint32_t)(status))
#define WMBUS_STATUS_MASK_ALL                                                                \
    (WMBUS_STATUS_MASK(WmBusStatusDecodeFail) | WMBUS_STATUS_MASK(WmBusStatusNotPlausible) | \
     WMBUS_STATUS_MASK(WmBusStatusFramingError) | WMBUS_STATUS_MASK(WmBusStatusCrcBad) |     \
     WMBUS_STATUS_MASK(WmBusStatusWeakRssi) | WMBUS_STATUS_MASK(WmBusStatusOk))

#define WMBUS_PACKET_PREVIEW_MAX 48U
#define WMBUS_PACKET_DETAIL_MAX  192U

static inline bool wmbus_status_mask_test(WmBusStatusMask mask, WmBusStatus status) {
    if((status == WmBusStatusNone) || (status >= WmBusStatusCount)) {
        return false;
    }

    return (mask & WMBUS_STATUS_MASK(status)) != 0U;
}

static inline WmBusStatus wmbus_status_threshold_clamp(WmBusStatus status) {
    if(status < WmBusStatusDecodeFail) return WmBusStatusDecodeFail;
    if(status >= WmBusStatusCount) return WmBusStatusOk;
    return status;
}

static inline bool wmbus_status_meets_threshold(WmBusStatus status, WmBusStatus threshold) {
    if(status == WmBusStatusNone || status >= WmBusStatusCount) {
        return false;
    }

    return status >= wmbus_status_threshold_clamp(threshold);
}
