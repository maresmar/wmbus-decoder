#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_spi.h>

#include <stdio.h>
#include <string.h>

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/view.h>

#include <lib/drivers/cc1101_regs.h>

#include "wmbus_view.h"
#include "wmbus_capture.h"

#define TAG "WmBusDecoder"

#define WMBUS_FREQ_HZ           868950000UL
#define WMBUS_DECODE_MAX        256
#define WMBUS_FIFO_CHUNK        64
#define WMBUS_RSSI_STRONG_DBM   (-70)
#define WMBUS_RSSI_UPDATE_HZ    5U
#define WMBUS_LED_PULSE_MS      40U
#define WMBUS_C_READ_TIMEOUT_MS 25U

#define WMBUS_MODE_COUNT 2U

static uint8_t wmbus_cc1101_preset_regs[];

static void wmbus_preset_set_reg(uint8_t reg, uint8_t value) {
    for(size_t i = 0;; i += 2) {
        if(wmbus_cc1101_preset_regs[i] == 0 && wmbus_cc1101_preset_regs[i + 1] == 0) break;
        if(wmbus_cc1101_preset_regs[i] == reg) {
            wmbus_cc1101_preset_regs[i + 1] = value;
            return;
        }
    }
}

static void wmbus_radio_apply_t_mode(void) {
    wmbus_preset_set_reg(CC1101_SYNC1, 0x54);
    wmbus_preset_set_reg(CC1101_SYNC0, 0x3D);
    wmbus_preset_set_reg(CC1101_IOCFG0, 0x00);
    // Return to IDLE after packet in T mode (software framing path controls RX explicitly).
    wmbus_preset_set_reg(CC1101_MCSM1, 0x00);
    wmbus_preset_set_reg(CC1101_PKTCTRL1, 0x00);
    // Infinite-length mode without whitening for T (3-of-6 path).
    wmbus_preset_set_reg(CC1101_PKTCTRL0, 0x02);
}

static void wmbus_radio_apply_c_mode(void) {
    wmbus_preset_set_reg(CC1101_SYNC1, 0x54);
    wmbus_preset_set_reg(CC1101_SYNC0, 0x3D);
    // GDO0: sync detect high during packet, low at packet end.
    wmbus_preset_set_reg(CC1101_IOCFG0, 0x06);
    // Stay in RX automatically after each packet in C mode.
    wmbus_preset_set_reg(CC1101_MCSM1, 0x0C);
    // Disable status append for parser-friendly payload bytes.
    wmbus_preset_set_reg(CC1101_PKTCTRL1, 0x00);
    // Variable-length packet mode + data whitening for C path.
    wmbus_preset_set_reg(CC1101_PKTCTRL0, 0x41);
}

static void wmbus_radio_apply_mode(WmBusRxMode mode) {
    if(mode == WmBusRxModeC) {
        wmbus_radio_apply_c_mode();
    } else {
        wmbus_radio_apply_t_mode();
    }

    // Reset avoids CC1101 state asserts seen with frequent reconfigure.
    furi_hal_subghz_reset();
    furi_hal_subghz_load_custom_preset(wmbus_cc1101_preset_regs);
    furi_hal_subghz_set_frequency_and_path(WMBUS_FREQ_HZ);
    furi_hal_subghz_flush_rx();
    furi_hal_subghz_rx();
}

static void wmbus_radio_recover_rx(void) {
    // Deterministic recovery: SIDLE -> SFRX -> SRX.
    // SFRX from RX state can be ignored on CC1101 and leave stale bytes in FIFO.
    furi_hal_subghz_idle();
    furi_hal_subghz_flush_rx();
    furi_hal_subghz_rx();
}

static uint8_t wmbus_radio_read_fifo_raw(uint8_t* data, uint8_t data_max) {
    if(!data || data_max == 0) return 0;

    uint8_t size = 0;
    bool overflow = false;
    uint8_t rxbytes_cmd[2] = {CC1101_STATUS_RXBYTES | CC1101_READ | CC1101_BURST, 0};

    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz);
    furi_hal_spi_bus_trx(
        &furi_hal_spi_bus_handle_subghz,
        rxbytes_cmd,
        rxbytes_cmd,
        sizeof(rxbytes_cmd),
        CC1101_TIMEOUT);
    overflow = ((rxbytes_cmd[1] & 0x80U) != 0U);
    size = (rxbytes_cmd[1] & 0x7FU);
    if(size > data_max) size = data_max;

    if(!overflow && size > 0) {
        uint8_t addr = CC1101_FIFO | CC1101_READ | CC1101_BURST;
        furi_hal_spi_bus_tx(&furi_hal_spi_bus_handle_subghz, &addr, 1, CC1101_TIMEOUT);
        furi_hal_spi_bus_rx(&furi_hal_spi_bus_handle_subghz, data, size, CC1101_TIMEOUT);
    }

    furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz);
    if(overflow) {
        wmbus_radio_recover_rx();
        size = 0;
    }

    return size;
}

static uint8_t wmbus_radio_get_rxbytes(bool* overflow) {
    uint8_t rxbytes_cmd[2] = {CC1101_STATUS_RXBYTES | CC1101_READ | CC1101_BURST, 0};
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz);
    furi_hal_spi_bus_trx(
        &furi_hal_spi_bus_handle_subghz,
        rxbytes_cmd,
        rxbytes_cmd,
        sizeof(rxbytes_cmd),
        CC1101_TIMEOUT);
    furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz);

    if(overflow) *overflow = ((rxbytes_cmd[1] & 0x80U) != 0U);
    return rxbytes_cmd[1] & 0x7FU;
}

// CC1101 register preset baseline for Wireless M-Bus (868.95 MHz, 100 kbps).
// Mode-specific fields (IOCFG0, PKTCTRL0, PKTCTRL1) are patched at runtime.
static uint8_t wmbus_cc1101_preset_regs[] = {
    // GPIO configuration
    CC1101_IOCFG2,
    0x06,
    CC1101_IOCFG1,
    0x2E,
    CC1101_IOCFG0,
    0x00,

    CC1101_FIFOTHR,
    0x07,

    // Sync word (0x54 0x3D)
    CC1101_SYNC1,
    0x54,
    CC1101_SYNC0,
    0x3D,

    CC1101_PKTLEN,
    0xFF,
    CC1101_PKTCTRL1,
    0x04,
    // Infinite length, no CRC. WM-Bus L-field is encoded, so we can't use it for HW length.
    CC1101_PKTCTRL0,
    0x02,

    CC1101_ADDR,
    0x00,
    CC1101_CHANNR,
    0x00,

    CC1101_FSCTRL1,
    0x08,
    CC1101_FSCTRL0,
    0x00,

    // Modem configuration (RX 103 kbps)
    CC1101_MDMCFG4,
    0x5C,
    CC1101_MDMCFG3,
    0x04,
    // Sync mode: 15/16, no carrier sense requirement.
    CC1101_MDMCFG2,
    0x05,
    CC1101_MDMCFG1,
    0x22,
    CC1101_MDMCFG0,
    0xF8,

    // Frequency deviation (RX 38 kHz)
    CC1101_DEVIATN,
    0x44,

    CC1101_MCSM2,
    0x07,
    CC1101_MCSM1,
    0x00,
    CC1101_MCSM0,
    0x18,

    CC1101_FOCCFG,
    0x2E,
    CC1101_BSCFG,
    0xBF,

    CC1101_AGCCTRL2,
    0x43,
    CC1101_AGCCTRL1,
    0x09,
    CC1101_AGCCTRL0,
    0xB5,

    CC1101_WOREVT1,
    0x87,
    CC1101_WOREVT0,
    0x6B,
    CC1101_WORCTRL,
    0xFB,

    CC1101_FREND1,
    0xB6,
    CC1101_FREND0,
    0x10,

    CC1101_FSCAL3,
    0xEA,
    CC1101_FSCAL2,
    0x2A,
    CC1101_FSCAL1,
    0x00,
    CC1101_FSCAL0,
    0x1F,

    CC1101_RCCTRL1,
    0x41,
    CC1101_RCCTRL0,
    0x00,

    CC1101_FSTEST,
    0x59,
    CC1101_PTEST,
    0x7F,
    CC1101_AGCTEST,
    0x3F,
    CC1101_TEST2,
    0x81,
    CC1101_TEST1,
    0x35,
    CC1101_TEST0,
    0x09,

    // End load reg
    0,
    0,

    // PATABLE (not used for RX, but required by loader)
    0xC0,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
};

typedef struct {
    ViewDispatcher* view_dispatcher;
    View* view;
    WmBusViewContext view_ctx;
    FuriThread* rx_thread;
    FuriMessageQueue* control_queue;
} WmBusApp;

static void wmbus_decode_mfg(uint16_t man, char out[4]) {
    out[0] = (char)(((man >> 10) & 0x1F) + ('A' - 1));
    out[1] = (char)(((man >> 5) & 0x1F) + ('A' - 1));
    out[2] = (char)((man & 0x1F) + ('A' - 1));
    out[3] = '\0';
    for(size_t i = 0; i < 3; i++) {
        if(out[i] < 'A' || out[i] > 'Z') {
            out[i] = '?';
        }
    }
}

static bool wmbus_format_id_bcd(const uint8_t id[4], char out[9]) {
    size_t pos = 0;
    for(int i = 3; i >= 0; i--) {
        uint8_t byte = id[i];
        uint8_t hi = (byte >> 4) & 0x0F;
        uint8_t lo = byte & 0x0F;
        if(hi > 9 || lo > 9) return false;
        out[pos++] = (char)('0' + hi);
        out[pos++] = (char)('0' + lo);
    }
    out[pos] = '\0';
    return true;
}

static bool wmbus_ci_has_short_tpl(uint8_t ci) {
    switch(ci) {
    case 0x5A:
    case 0x61:
    case 0x65:
    case 0x67:
    case 0x6E:
    case 0x74:
    case 0x7A:
    case 0x7D:
    case 0x7F:
    case 0x8A:
    case 0x9E:
        return true;
    default:
        return false;
    }
}

static uint16_t wmbus_crc16_en13757(const uint8_t* data, size_t len) {
    uint16_t crc = 0x0000;
    for(size_t i = 0; i < len; i++) {
        uint8_t b = data[i];
        for(uint8_t bit = 0; bit < 8; bit++) {
            if((((crc & 0x8000) >> 8) ^ (b & 0x80)) != 0) {
                crc = (uint16_t)((crc << 1) ^ 0x3D65);
            } else {
                crc = (uint16_t)(crc << 1);
            }
            b <<= 1;
        }
    }
    return (uint16_t)(~crc);
}

static bool wmbus_crc_check_frame_a(const uint8_t* data, size_t len) {
    if(len < 12) return false;

    uint16_t calc = wmbus_crc16_en13757(data, 10);
    uint16_t check = ((uint16_t)data[10] << 8) | data[11];
    if(calc != check) return false;

    size_t pos = 12;
    while(pos + 18 <= len) {
        size_t to = pos + 16;
        calc = wmbus_crc16_en13757(&data[pos], 16);
        check = ((uint16_t)data[to] << 8) | data[to + 1];
        if(calc != check) return false;
        pos += 18;
    }

    if(pos < len - 2) {
        size_t tto = len - 2;
        size_t blen = tto - pos;
        calc = wmbus_crc16_en13757(&data[pos], blen);
        check = ((uint16_t)data[tto] << 8) | data[tto + 1];
        if(calc != check) return false;
    }

    return true;
}

static bool wmbus_crc_check_frame_b(const uint8_t* data, size_t len) {
    if(len < 12) return false;

    size_t crc1_pos = 0;
    size_t crc2_pos = 0;
    if(len <= 128) {
        crc1_pos = len - 2;
    } else {
        crc1_pos = 126;
        crc2_pos = len - 2;
    }

    uint16_t calc = wmbus_crc16_en13757(data, crc1_pos);
    uint16_t check = ((uint16_t)data[crc1_pos] << 8) | data[crc1_pos + 1];
    if(calc != check) return false;

    if(crc2_pos > 0) {
        size_t from2 = crc1_pos + 2;
        size_t len2 = crc2_pos - from2;
        calc = wmbus_crc16_en13757(&data[from2], len2);
        check = ((uint16_t)data[crc2_pos] << 8) | data[crc2_pos + 1];
        if(calc != check) return false;
    }

    return true;
}

static bool wmbus_crc_ok(const uint8_t* data, size_t len, bool used_3of6) {
    if(len < 12) return false;
    uint8_t l_field = data[0];
    if(used_3of6) {
        size_t len_a = wmbus_capture_frame_len_format_a(l_field);
        if(len_a <= len) return wmbus_crc_check_frame_a(data, len_a);
    } else {
        size_t len_b = wmbus_capture_frame_len_format_b(l_field);
        if(len_b <= len) return wmbus_crc_check_frame_b(data, len_b);
    }
    return false;
}

static uint8_t wmbus_calc_confidence(
    bool decoded_ok,
    bool plausible,
    bool length_ok,
    bool crc_ok,
    bool strong_rssi) {
    uint8_t score = 0;
    if(decoded_ok) score += 15;
    if(plausible) score += 25;
    if(length_ok) score += 20;
    if(crc_ok) score += 30;
    if(strong_rssi) score += 10;
    if(score > 100) score = 100;
    return score;
}

static void wmbus_rssi_hist_push(WmBusViewModel* model, int rssi) {
    uint8_t next = 0;
    if(model->rssi_hist_count == 0) {
        next = 0;
    } else {
        next = (uint8_t)((model->rssi_hist_head + 1U) % WMBUS_RSSI_HISTORY);
    }
    model->rssi_hist_head = next;
    if(model->rssi_hist_count < WMBUS_RSSI_HISTORY) {
        model->rssi_hist_count++;
    }
    model->rssi_hist[model->rssi_hist_head] = (int8_t)rssi;
}

static void wmbus_history_push(
    WmBusViewModel* model,
    const uint8_t* frame,
    size_t frame_len,
    bool used_3of6,
    int rssi,
    bool crc_ok,
    bool has_total_m3,
    uint32_t total_m3_x1000) {
    uint8_t next = 0;
    if(model->hist_count == 0) {
        next = 0;
    } else {
        next = (uint8_t)((model->hist_head + 1U) % WMBUS_HIST_MAX);
    }
    model->hist_head = next;
    if(model->hist_count < WMBUS_HIST_MAX) {
        model->hist_count++;
    }

    WmBusHistoryEntry* entry = &model->hist[model->hist_head];
    memset(entry, 0, sizeof(*entry));
    entry->l_field = frame[0];
    entry->c_field = frame[1];
    uint16_t man = (uint16_t)frame[2] | ((uint16_t)frame[3] << 8);
    wmbus_decode_mfg(man, entry->mfg);
    memcpy(entry->id_str, "????????", 9);
    if(!wmbus_format_id_bcd(&frame[4], entry->id_str)) {
        snprintf(
            entry->id_str,
            sizeof(entry->id_str),
            "%02X%02X%02X%02X",
            frame[7],
            frame[6],
            frame[5],
            frame[4]);
    }
    entry->version = frame[8];
    entry->dev_type = frame[9];
    entry->ci_field = frame[10];
    entry->rssi = (int8_t)rssi;
    entry->crc_ok = crc_ok;
    entry->used_3of6 = used_3of6;
    entry->has_total_m3 = has_total_m3;
    entry->total_m3_x1000 = total_m3_x1000;
    entry->frame_preview_len =
        (uint8_t)((frame_len < WMBUS_FRAME_PREVIEW_MAX) ? frame_len : WMBUS_FRAME_PREVIEW_MAX);
    memcpy(entry->frame_preview, frame, entry->frame_preview_len);

    if(model->freeze_display) {
        if(model->hist_count > 0) {
            uint8_t max_cursor = (uint8_t)(model->hist_count - 1U);
            if(model->hist_cursor < max_cursor) {
                model->hist_cursor++;
            }
        }
    } else {
        model->hist_cursor = 0;
    }
}

static void wmbus_update_model_locked(
    WmBusViewModel* model,
    const uint8_t* frame,
    size_t frame_len,
    bool used_3of6,
    uint8_t raw_len,
    int rssi,
    bool crc_ok,
    bool length_ok) {
    if(frame_len < 11) return;

    model->has_packet = true;
    model->used_3of6 = used_3of6;
    model->raw_len = raw_len;
    model->decoded_len = (uint16_t)frame_len;
    model->rssi = rssi;

    model->l_field = frame[0];
    model->c_field = frame[1];
    model->m_field = (uint16_t)frame[2] | ((uint16_t)frame[3] << 8);
    wmbus_decode_mfg(model->m_field, model->mfg);

    memcpy(model->id, &frame[4], 4);
    model->id_is_bcd = wmbus_format_id_bcd(model->id, model->id_str);
    if(!model->id_is_bcd) {
        snprintf(
            model->id_str,
            sizeof(model->id_str),
            "%02X%02X%02X%02X",
            model->id[3],
            model->id[2],
            model->id[1],
            model->id[0]);
    }

    model->version = frame[8];
    model->dev_type = frame[9];
    model->ci_field = frame[10];

    model->has_short_tpl = false;
    if(wmbus_ci_has_short_tpl(model->ci_field) && frame_len >= 15) {
        model->has_short_tpl = true;
        model->acc = frame[11];
        model->status = frame[12];
        model->cfg = (uint16_t)frame[13] | ((uint16_t)frame[14] << 8);
    }

    model->has_total_m3 = false;
    model->total_m3_x1000 = 0;
    if(wmbus_parser_parse_apator162_total(frame, frame_len, &model->total_m3_x1000)) {
        model->has_total_m3 = true;
    }

    model->length_ok = length_ok;
    wmbus_history_push(
        model,
        frame,
        frame_len,
        used_3of6,
        rssi,
        crc_ok,
        model->has_total_m3,
        model->total_m3_x1000);
}

static bool wmbus_navigation_callback(void* context) {
    furi_assert(context);
    WmBusApp* app = context;
    view_dispatcher_stop(app->view_dispatcher);
    return true;
}

static bool wmbus_capture_t_step(
    WmBusCaptureStateT* state,
    WmBusCaptureFrame* frame,
    bool* had_data,
    uint32_t gap_ticks) {
    furi_check(state);
    furi_check(frame);
    furi_check(had_data);

    bool force_complete = false;
    uint8_t temp[WMBUS_FIFO_CHUNK];

    while(true) {
        uint8_t chunk_len = wmbus_radio_read_fifo_raw(temp, sizeof(temp));
        if(chunk_len == 0) break;

        size_t copy = sizeof(state->raw) - state->raw_len;
        if(copy > 0) {
            if(chunk_len < copy) copy = chunk_len;
            memcpy(&state->raw[state->raw_len], temp, copy);
            state->raw_len += copy;
        }

        *had_data = true;
        state->in_packet = true;
        state->last_byte_tick = furi_get_tick();
        if(state->raw_len >= sizeof(state->raw)) break;

        if(state->expected_raw_len == 0) {
            size_t expected_raw_len = 0;
            if(wmbus_capture_estimate_t_expected_raw_len(
                   state->raw, state->raw_len, sizeof(state->raw), &expected_raw_len)) {
                state->expected_raw_len = expected_raw_len;
            }
        }

        if(state->expected_raw_len > 0 && state->raw_len >= state->expected_raw_len) {
            force_complete = true;
            break;
        }
    }

    if(state->in_packet) {
        uint32_t now = furi_get_tick();
        bool gap = force_complete || (!*had_data && (now - state->last_byte_tick) >= gap_ticks);
        bool full = (state->raw_len >= sizeof(state->raw));

        if((gap || full) && state->raw_len > 0) {
            size_t frame_raw_len = state->raw_len;
            if(state->expected_raw_len > 0 && frame_raw_len > state->expected_raw_len) {
                frame_raw_len = state->expected_raw_len;
            }

            memcpy(frame->data, state->raw, frame_raw_len);
            frame->len = frame_raw_len;
            frame->raw_len = frame_raw_len;
            frame->rssi = (int)furi_hal_subghz_get_rssi();
            frame->mode = WmBusRxModeT;

            wmbus_capture_state_t_reset(state);
            wmbus_radio_recover_rx();
            return true;
        }
    }

    return false;
}

static bool wmbus_capture_c_step(
    WmBusCaptureStateC* state,
    WmBusCaptureFrame* frame,
    bool* had_data) {
    furi_check(state);
    furi_check(frame);
    furi_check(had_data);

    bool overflow = false;
    uint8_t rxbytes = wmbus_radio_get_rxbytes(&overflow);
    if(overflow) {
        state->dropped_invalid++;
        wmbus_radio_recover_rx();
        return false;
    }
    if(rxbytes == 0) return false;

    // Variable-length mode packet format in FIFO: [LEN][PAYLOAD...]
    uint8_t payload_len_byte = 0;
    if(wmbus_radio_read_fifo_raw(&payload_len_byte, 1) != 1) {
        state->dropped_invalid++;
        wmbus_radio_recover_rx();
        return false;
    }
    *had_data = true;

    size_t payload_len = payload_len_byte;
    if(payload_len == 0 || payload_len > (WMBUS_DECODE_MAX - 1U)) {
        state->dropped_oversize++;
        wmbus_radio_recover_rx();
        return false;
    }

    uint8_t payload[WMBUS_DECODE_MAX] = {0};
    size_t payload_read = 0;
    uint32_t timeout_ticks =
        (furi_kernel_get_tick_frequency() * WMBUS_C_READ_TIMEOUT_MS + 999U) / 1000U;
    if(timeout_ticks < 1U) timeout_ticks = 1U;
    uint32_t last_data_tick = furi_get_tick();

    while(payload_read < payload_len) {
        size_t left = payload_len - payload_read;
        uint8_t chunk =
            wmbus_radio_read_fifo_raw(&payload[payload_read], (left > 255U) ? 255U : (uint8_t)left);
        if(chunk > 0) {
            payload_read += chunk;
            last_data_tick = furi_get_tick();
            continue;
        }

        uint32_t now_tick = furi_get_tick();
        if((now_tick - last_data_tick) >= timeout_ticks) break;
        furi_delay_ms(1);
    }

    if(payload_read != payload_len) {
        state->dropped_invalid++;
        wmbus_radio_recover_rx();
        return false;
    }

    size_t frame_len = 0;
    if(!wmbus_capture_reconstruct_c_frame(
           payload, payload_len, frame->data, sizeof(frame->data), &frame_len)) {
        state->dropped_invalid++;
        wmbus_radio_recover_rx();
        return false;
    }

    rxbytes = wmbus_radio_get_rxbytes(&overflow);
    if(overflow) {
        (void)rxbytes;
        state->dropped_invalid++;
        wmbus_radio_recover_rx();
        return false;
    }

    frame->len = frame_len;
    frame->raw_len = payload_len + 1U;
    frame->rssi = (int)furi_hal_subghz_get_rssi();
    frame->mode = WmBusRxModeC;

    // In C mode we configure RXOFF_MODE=RX, so packet engine should already continue receiving.
    // Avoid unconditional SRX here to prevent asserts when chip temporarily enters overflow state.
    return true;
}

static void wmbus_process_captured_frame(WmBusApp* app, const WmBusCaptureFrame* capture) {
    uint8_t decoded[WMBUS_DECODE_MAX] = {0};
    size_t decoded_len = 0;
    bool decoded_ok = false;
    bool plausible = false;
    const uint8_t* frame = NULL;
    size_t frame_len = 0;
    bool used_3of6 = (capture->mode == WmBusRxModeT);
    bool length_ok = false;
    bool crc_ok = false;
    bool crc_known = false;

    if(used_3of6) {
        decoded_ok = wmbus_parser_decode_3of6(
            capture->data, capture->len, decoded, sizeof(decoded), &decoded_len);
        if(decoded_ok && wmbus_parser_is_plausible(decoded, decoded_len)) {
            frame = decoded;
            frame_len = decoded_len;
            plausible = true;
        }
    } else {
        decoded_ok = true;
        if(wmbus_parser_is_plausible(capture->data, capture->len)) {
            frame = capture->data;
            frame_len = capture->len;
            plausible = true;
        }
    }

    if(plausible) {
        uint8_t l_field = frame[0];
        size_t expected_len = used_3of6 ? wmbus_capture_frame_len_format_a(l_field) :
                                          wmbus_capture_frame_len_format_b(l_field);
        if(frame_len >= expected_len) {
            frame_len = expected_len;
            length_ok = true;
            crc_ok = wmbus_crc_ok(frame, frame_len, used_3of6);
            crc_known = true;
        }
    }

    bool strong_rssi = (capture->rssi >= WMBUS_RSSI_STRONG_DBM);
    WmBusStatus status = WmBusStatusNone;
    if(used_3of6 && !decoded_ok) {
        status = WmBusStatusDecodeFail;
    } else if(!plausible) {
        status = WmBusStatusNotPlausible;
    } else if(!length_ok) {
        status = WmBusStatusFramingError;
    } else if(crc_known && !crc_ok) {
        status = WmBusStatusCrcBad;
    } else if(!strong_rssi) {
        status = WmBusStatusWeakRssi;
    } else {
        status = WmBusStatusOk;
    }

    uint8_t confidence =
        wmbus_calc_confidence(decoded_ok, plausible, length_ok, crc_ok, strong_rssi);

    uint32_t now_tick = furi_get_tick();
    uint32_t tick_freq = furi_kernel_get_tick_frequency();
    int rssi = capture->rssi;

    with_view_model(
        app->view,
        WmBusViewModel * model,
        {
            model->packets_seen++;
            model->last_raw_len = (uint16_t)capture->raw_len;
            model->rssi = rssi;
            model->last_status = status;
            model->last_confidence = confidence;

            if(strong_rssi) {
                model->packets_strong++;
            }

            if(plausible) {
                model->packets_decoded++;
                if(crc_known) {
                    if(crc_ok) {
                        model->packets_crc_ok++;
                    } else {
                        model->packets_crc_bad++;
                    }
                }
                model->last_crc_valid = crc_known;
                model->last_crc_ok = crc_ok;
                wmbus_update_model_locked(
                    model,
                    frame,
                    frame_len,
                    used_3of6,
                    (uint8_t)capture->raw_len,
                    rssi,
                    crc_ok,
                    length_ok);
            } else {
                model->last_crc_valid = false;
                model->last_crc_ok = false;
            }

            if(model->rate_last_tick == 0) {
                model->rate_last_tick = now_tick;
                model->rate_last_seen = model->packets_seen;
                model->packets_per_sec = 0;
            } else if((now_tick - model->rate_last_tick) >= tick_freq) {
                uint32_t elapsed = now_tick - model->rate_last_tick;
                uint32_t diff = model->packets_seen - model->rate_last_seen;
                model->packets_per_sec = (uint16_t)((diff * tick_freq) / (elapsed ? elapsed : 1U));
                model->rate_last_tick = now_tick;
                model->rate_last_seen = model->packets_seen;
            }

            wmbus_rssi_hist_push(model, rssi);
        },
        true);
}

static int32_t wmbus_rx_thread(void* context) {
    WmBusApp* app = context;

    if(!furi_hal_subghz_is_frequency_valid(WMBUS_FREQ_HZ)) {
        with_view_model(app->view, WmBusViewModel * model, { model->freq_valid = false; }, true);
        return 0;
    }

    furi_hal_power_suppress_charge_enter();

    WmBusRxMode mode = WmBusRxModeT;
    uint8_t sync_index = 0;
    wmbus_radio_apply_mode(mode);

    WmBusCaptureStateT capture_t = {0};
    WmBusCaptureStateC capture_c = {0};
    wmbus_capture_state_t_reset(&capture_t);
    wmbus_capture_state_c_reset(&capture_c);

    uint32_t gap_ticks = furi_kernel_get_tick_frequency() / 200;
    uint32_t rssi_ticks = furi_kernel_get_tick_frequency() / WMBUS_RSSI_UPDATE_HZ;
    uint32_t led_pulse_ticks =
        (furi_kernel_get_tick_frequency() * WMBUS_LED_PULSE_MS + 999U) / 1000U;
    uint32_t last_rssi_tick = 0;
    uint32_t led_pulse_off_tick = 0;
    bool led_pulse_on = false;
    if(gap_ticks < 1) gap_ticks = 1;
    if(rssi_ticks < 1) rssi_ticks = 1;
    if(led_pulse_ticks < 1) led_pulse_ticks = 1;

    bool running = true;
    while(running) {
        WmBusControlCmd command;
        while(furi_message_queue_get(app->control_queue, &command, 0) == FuriStatusOk) {
            if(command == WmBusControlCmdStop) {
                running = false;
                break;
            }

            uint8_t requested_sync = sync_index;
            if(command == WmBusControlCmdSetModeT) {
                requested_sync = 0;
            } else if(command == WmBusControlCmdSetModeC) {
                requested_sync = 1;
            }

            if(requested_sync < WMBUS_MODE_COUNT && requested_sync != sync_index) {
                sync_index = requested_sync;
                mode = (sync_index == 0) ? WmBusRxModeT : WmBusRxModeC;
                wmbus_radio_apply_mode(mode);
                wmbus_capture_state_t_reset(&capture_t);
                wmbus_capture_state_c_reset(&capture_c);
                with_view_model(
                    app->view, WmBusViewModel * model, { model->sync_index = sync_index; }, true);
            }
        }

        if(!running) break;

        bool had_data = false;
        WmBusCaptureFrame capture = {0};
        bool frame_ready = false;

        if(mode == WmBusRxModeT) {
            frame_ready = wmbus_capture_t_step(&capture_t, &capture, &had_data, gap_ticks);
        } else {
            frame_ready = wmbus_capture_c_step(&capture_c, &capture, &had_data);
        }

        if(frame_ready) {
            uint32_t pulse_now = furi_get_tick();
            furi_hal_light_set(LightGreen, 0xFF);
            led_pulse_on = true;
            led_pulse_off_tick = pulse_now + led_pulse_ticks;
            wmbus_process_captured_frame(app, &capture);
        }

        uint32_t now_tick = furi_get_tick();
        if(led_pulse_on && ((int32_t)(now_tick - led_pulse_off_tick) >= 0)) {
            furi_hal_light_set(LightGreen, 0x00);
            led_pulse_on = false;
        }
        if(last_rssi_tick == 0 || (now_tick - last_rssi_tick) >= rssi_ticks) {
            int rssi = (int)furi_hal_subghz_get_rssi();
            with_view_model(app->view, WmBusViewModel * model, { model->rssi = rssi; }, true);
            last_rssi_tick = now_tick;
        }

        if(!had_data) {
            furi_delay_ms(1);
        }
    }

    furi_hal_light_set(LightGreen, 0x00);
    furi_hal_subghz_sleep();
    furi_hal_power_suppress_charge_exit();
    return 0;
}

static WmBusApp* wmbus_app_alloc(void) {
    WmBusApp* app = malloc(sizeof(WmBusApp));
    memset(app, 0, sizeof(WmBusApp));

    app->control_queue = furi_message_queue_alloc(8, sizeof(WmBusControlCmd));
    furi_check(app->control_queue);

    app->view = view_alloc();
    view_allocate_model(app->view, ViewModelTypeLocking, sizeof(WmBusViewModel));

    with_view_model(
        app->view,
        WmBusViewModel * model,
        {
            memset(model, 0, sizeof(WmBusViewModel));
            model->freq_valid = true;
            model->sync_index = 0;
        },
        false);

    app->view_dispatcher = view_dispatcher_alloc();
    app->view_ctx.view = app->view;
    app->view_ctx.view_dispatcher = app->view_dispatcher;
    app->view_ctx.control_queue = app->control_queue;
    wmbus_view_setup(app->view, &app->view_ctx);
    view_dispatcher_add_view(app->view_dispatcher, 0, app->view);

    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, wmbus_navigation_callback);

    app->rx_thread = furi_thread_alloc_ex("WmBusRx", 4096, wmbus_rx_thread, app);
    furi_thread_start(app->rx_thread);

    return app;
}

static void wmbus_app_free(WmBusApp* app) {
    if(app->rx_thread) {
        WmBusControlCmd stop = WmBusControlCmdStop;
        furi_message_queue_put(app->control_queue, &stop, FuriWaitForever);
        furi_thread_join(app->rx_thread);
        furi_thread_free(app->rx_thread);
    }

    view_dispatcher_remove_view(app->view_dispatcher, 0);
    view_dispatcher_free(app->view_dispatcher);
    view_free(app->view);
    furi_message_queue_free(app->control_queue);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t wmbus_decoder_app(void* arg) {
    UNUSED(arg);

    WmBusApp* app = wmbus_app_alloc();
    view_dispatcher_switch_to_view(app->view_dispatcher, 0);
    view_dispatcher_run(app->view_dispatcher);
    wmbus_app_free(app);

    return 0;
}
