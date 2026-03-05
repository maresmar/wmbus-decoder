#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <gui/view.h>
#include <gui/view_dispatcher.h>

#define WMBUS_HIST_MAX          20
#define WMBUS_FRAME_PREVIEW_MAX 32
#define WMBUS_RSSI_HISTORY      64

typedef enum {
    WmBusStatusNone = 0,
    WmBusStatusDecodeFail,
    WmBusStatusNotPlausible,
    WmBusStatusFramingError,
    WmBusStatusCrcBad,
    WmBusStatusWeakRssi,
    WmBusStatusOk,
} WmBusStatus;

typedef struct {
    uint8_t l_field;
    uint8_t c_field;
    char mfg[4];
    char id_str[9];
    uint8_t version;
    uint8_t dev_type;
    uint8_t ci_field;
    int8_t rssi;
    bool crc_ok;
    bool used_3of6;
    uint8_t frame_preview_len;
    uint8_t frame_preview[WMBUS_FRAME_PREVIEW_MAX];
} WmBusHistoryEntry;

typedef struct {
    bool has_packet;
    bool used_3of6;
    bool length_ok;

    uint8_t l_field;
    uint8_t c_field;
    uint16_t m_field;
    char mfg[4];

    uint8_t id[4];
    char id_str[9];
    bool id_is_bcd;

    uint8_t version;
    uint8_t dev_type;
    uint8_t ci_field;

    bool has_short_tpl;
    uint8_t acc;
    uint8_t status;
    uint16_t cfg;

    uint16_t raw_len;
    uint16_t decoded_len;
    int rssi;

    bool freq_valid;
    bool debug_mode;
    WmBusStatus last_status;
    uint8_t last_confidence;
    uint32_t rate_last_tick;
    uint32_t rate_last_seen;
    uint16_t packets_per_sec;

    WmBusHistoryEntry hist[WMBUS_HIST_MAX];
    uint8_t hist_count;
    uint8_t hist_head;
    uint8_t hist_cursor;
    bool freeze_display;

    int8_t rssi_hist[WMBUS_RSSI_HISTORY];
    uint8_t rssi_hist_count;
    uint8_t rssi_hist_head;

    uint16_t last_raw_len;
    uint32_t packets_seen;
    uint32_t packets_decoded;
    uint32_t packets_strong;
    uint32_t packets_crc_ok;
    uint32_t packets_crc_bad;
    bool last_crc_valid;
    bool last_crc_ok;
    uint8_t sync_index;
} WmBusViewModel;

typedef struct {
    ViewDispatcher* view_dispatcher;
    View* view;
    volatile bool* mode_change;
    volatile uint8_t* mode_request;
} WmBusViewContext;

void wmbus_view_setup(View* view, WmBusViewContext* ctx);
