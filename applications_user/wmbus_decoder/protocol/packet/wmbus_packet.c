#include "wmbus_packet.h"

#include <furi.h>
#include <string.h>

#include "wmbus_packet_decode.h"
#include "wmbus_packet_parser.h"
#include "wmbus_packet_security.h"

const char* wmbus_packet_status_str(WmBusStatus status) {
    switch(status) {
    case WmBusStatusDecodeFail:
        return "Decode fail";
    case WmBusStatusNotPlausible:
        return "Not plausible";
    case WmBusStatusFramingError:
        return "Framing error";
    case WmBusStatusCrcBad:
        return "CRC bad";
    case WmBusStatusWeakRssi:
        return "Weak RSSI";
    case WmBusStatusOk:
        return "OK";
    case WmBusStatusParsed:
        return "Decoded";
    default:
        return "--";
    }
}

const char* wmbus_packet_status_short_label(WmBusStatus status) {
    switch(status) {
    case WmBusStatusDecodeFail:
        return "Decode";
    case WmBusStatusNotPlausible:
        return "Header";
    case WmBusStatusFramingError:
        return "Framing";
    case WmBusStatusCrcBad:
        return "CRC";
    case WmBusStatusWeakRssi:
        return "Weak RSSI";
    case WmBusStatusOk:
        return "OK";
    case WmBusStatusParsed:
        return "Decoded";
    default:
        return "--";
    }
}

const char* wmbus_packet_csv_logging_str(WmBusCsvLogging logging) {
    switch(logging) {
    case WmBusCsvLoggingBasic:
        return "Basic";
    case WmBusCsvLoggingFull:
        return "Full";
    case WmBusCsvLoggingNone:
    default:
        return "Off";
    }
}

const char* wmbus_packet_quality_str(WmBusPacketQuality quality) {
    switch(wmbus_packet_quality_clamp(quality)) {
    case WmBusPacketQualityAnyCapture:
        return "Any capture";
    case WmBusPacketQualityHeaderOk:
        return "Header OK";
    case WmBusPacketQualityFrameComplete:
        return "Frame complete";
    case WmBusPacketQualityCrcOk:
        return "CRC OK";
    case WmBusPacketQualityParsed:
        return "Decoded";
    default:
        return "--";
    }
}

const char* wmbus_packet_quality_short_label(WmBusPacketQuality quality) {
    switch(wmbus_packet_quality_clamp(quality)) {
    case WmBusPacketQualityAnyCapture:
        return "RX";
    case WmBusPacketQualityHeaderOk:
        return "HDR";
    case WmBusPacketQualityFrameComplete:
        return "LEN";
    case WmBusPacketQualityCrcOk:
        return "CRC"
    case WmBusPacketQualityParsed:
        return "DEC";
    default:
        return "--";
    }
}

WmBusPacketQuality wmbus_packet_quality_from_record(const WmBusPacketRecord* record) {
    if(!record || !record->has_capture) return WmBusPacketQualityAnyCapture;
    if(record->parsed_ok) return WmBusPacketQualityParsed;
    if(record->crc_known && record->crc_ok) return WmBusPacketQualityCrcOk;
    if(record->length_ok) return WmBusPacketQualityFrameComplete;
    if(record->header_ok) return WmBusPacketQualityHeaderOk;
    return WmBusPacketQualityAnyCapture;
}

bool wmbus_packet_record_passes_policy(
    const WmBusPacketRecord* record,
    WmBusPacketQuality min_quality,
    int32_t min_rssi_dbm) {
    if(!record || !record->has_capture) return false;
    if(min_rssi_dbm < 0 && record->rssi < min_rssi_dbm) return false;
    return wmbus_packet_quality_meets(record->quality, min_quality);
}

bool wmbus_packet_process_capture(
    const WmBusCaptureFrame* capture,
    const WmBusCryptoKeyStore* key_store,
    WmBusPacketRecord* record) {
    if(!capture || !record) return false;

    memset(record, 0, sizeof(*record));
    record->mode = capture->mode;
    record->raw_len = (uint16_t)capture->raw_len;
    record->capture_len =
        (uint16_t)((capture->len > sizeof(record->capture_bytes)) ? sizeof(record->capture_bytes) :
                                                                capture->len);
    record->best_offset = -1;
    record->rssi = capture->rssi;
    record->rx_tick = furi_get_tick();
    record->strong_rssi = true;
    record->rssi_ok = true;
    record->has_capture = (record->capture_len > 0U);
    memcpy(record->capture_bytes, capture->data, record->capture_len);

    uint8_t normalized[256] = {0};
    WmBusPacketDecodeState decode = {0};
    if(!wmbus_packet_decode_capture(capture, record, normalized, sizeof(normalized), &decode)) {
        return false;
    }

    bool parser_succeeded = false;
    if(record->plausible && decode.frame && decode.frame_len > 0U) {
        wmbus_packet_store_frame(record, decode.frame, decode.frame_len);
        wmbus_packet_resolve_application_payload(decode.frame, decode.frame_len, record, key_store);
        parser_succeeded = wmbus_packet_parse_application(record);
        wmbus_packet_finalize_parser(record);
    } else {
        record->packet_is_frame = false;
        record->packet_len = (uint16_t)((capture->len > sizeof(record->packet_bytes)) ?
                                            sizeof(record->packet_bytes) :
                                            capture->len);
        memcpy(record->packet_bytes, capture->data, record->packet_len);
        record->application.parser_id = WmBusParserIdRaw;
    }

    record->header_ok = record->plausible;
    record->parsed_ok = parser_succeeded || (record->application.record_count > 0U) ||
                        record->tpl.decrypted || record->ell.decrypted;
    record->quality = wmbus_packet_quality_from_record(record);

    if(decode.used_3of6 && !record->decoded_ok) {
        record->status = WmBusStatusDecodeFail;
    } else if(!record->plausible) {
        record->status = WmBusStatusNotPlausible;
    } else if(!record->length_ok) {
        record->status = WmBusStatusFramingError;
    } else if(record->crc_known && !record->crc_ok) {
        record->status = WmBusStatusCrcBad;
    } else if(record->parsed_ok) {
        record->status = WmBusStatusParsed;
    } else {
        record->status = WmBusStatusOk;
    }

    return true;
}
