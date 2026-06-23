#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    WmBusRxModeT = 0,
    WmBusRxModeC = 1,
} WmBusRxMode;

typedef enum {
    WmBusFrameFormatA = 0,
    WmBusFrameFormatB = 1,
} WmBusFrameFormat;

typedef enum {
    WmBusStatusNone = 0,
    WmBusStatusDecodeFail,
    WmBusStatusNotPlausible,
    WmBusStatusFramingError,
    WmBusStatusCrcBad,
    WmBusStatusWeakRssi,
    WmBusStatusOk,
    WmBusStatusParsed,
    WmBusStatusCount,
} WmBusStatus;

typedef enum {
    WmBusPacketQualityAnyCapture = 0,
    WmBusPacketQualityHeaderOk,
    WmBusPacketQualityFrameComplete,
    WmBusPacketQualityCrcOk,
    WmBusPacketQualityParsed,
    WmBusPacketQualityCount,
} WmBusPacketQuality;

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

#define WMBUS_PACKET_DETAIL_MAX  320U

static inline WmBusPacketQuality wmbus_packet_quality_clamp(WmBusPacketQuality quality) {
    if(quality >= WmBusPacketQualityCount) return WmBusPacketQualityAnyCapture;
    return quality;
}

static inline bool
    wmbus_packet_quality_meets(WmBusPacketQuality quality, WmBusPacketQuality threshold) {
    return wmbus_packet_quality_clamp(quality) >= wmbus_packet_quality_clamp(threshold);
}
