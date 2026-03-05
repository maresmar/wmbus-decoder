#include <furi.h>
#include <furi_hal.h>

#include <stdio.h>
#include <string.h>

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/view.h>

#include <lib/drivers/cc1101_regs.h>

#include "wmbus_view.h"

#define TAG "WmBusDecoder"

#define WMBUS_FREQ_HZ           868950000UL
#define WMBUS_RAW_MAX           256
#define WMBUS_DECODE_MAX        256
#define WMBUS_FIFO_CHUNK        64
#define WMBUS_RSSI_STRONG_DBM   (-70)
#define WMBUS_SYNC_ROTATE_TICKS (furi_kernel_get_tick_frequency() * 2U)

typedef struct {
    uint8_t sync1;
    uint8_t sync0;
    bool whitening;
    const char* label;
} WmBusSyncConfig;

static const WmBusSyncConfig wmbus_sync_list[] = {
    {0x54, 0x3D, false, "T"},
    {0x54, 0x3D, true, "C"},
};

static uint8_t wmbus_cc1101_preset_regs[];

static void wmbus_preset_set_sync(uint8_t sync1, uint8_t sync0) {
    for(size_t i = 0;; i += 2) {
        if(wmbus_cc1101_preset_regs[i] == 0 && wmbus_cc1101_preset_regs[i + 1] == 0) break;
        if(wmbus_cc1101_preset_regs[i] == CC1101_SYNC1) {
            wmbus_cc1101_preset_regs[i + 1] = sync1;
        } else if(wmbus_cc1101_preset_regs[i] == CC1101_SYNC0) {
            wmbus_cc1101_preset_regs[i + 1] = sync0;
        }
    }
}

static void wmbus_preset_set_whitening(bool enable) {
    uint8_t pktctrl0 = 0x02; // infinite length, no CRC
    if(enable) pktctrl0 |= 0x40; // data whitening

    for(size_t i = 0;; i += 2) {
        if(wmbus_cc1101_preset_regs[i] == 0 && wmbus_cc1101_preset_regs[i + 1] == 0) break;
        if(wmbus_cc1101_preset_regs[i] == CC1101_PKTCTRL0) {
            wmbus_cc1101_preset_regs[i + 1] = pktctrl0;
        }
    }
}

static void wmbus_radio_apply(uint8_t sync_index) {
    wmbus_preset_set_sync(wmbus_sync_list[sync_index].sync1, wmbus_sync_list[sync_index].sync0);
    wmbus_preset_set_whitening(wmbus_sync_list[sync_index].whitening);

    // Reset avoids CC1101 state asserts seen with frequent reconfigure.
    furi_hal_subghz_reset();
    furi_hal_subghz_load_custom_preset(wmbus_cc1101_preset_regs);
    furi_hal_subghz_set_frequency_and_path(WMBUS_FREQ_HZ);
    furi_hal_subghz_flush_rx();
    furi_hal_subghz_rx();
}

// CC1101 register preset for Wireless M-Bus (Radio Link B, 868.95 MHz, 100 kbps)
// Based on TI AN067 / SWRA234 (Appendix D), with IOCFG0 set to 0x06 so GDO0
// asserts on sync and deasserts at end-of-packet for our RX loop.
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
    volatile bool running;
    volatile bool mode_change;
    volatile uint8_t mode_request;
} WmBusApp;

static uint8_t wmbus_3of6_decode_symbol(uint8_t sym) {
    switch(sym) {
    case 0x16:
        return 0x0;
    case 0x0D:
        return 0x1;
    case 0x0E:
        return 0x2;
    case 0x0B:
        return 0x3;
    case 0x1C:
        return 0x4;
    case 0x19:
        return 0x5;
    case 0x1A:
        return 0x6;
    case 0x13:
        return 0x7;
    case 0x2C:
        return 0x8;
    case 0x25:
        return 0x9;
    case 0x26:
        return 0xA;
    case 0x23:
        return 0xB;
    case 0x34:
        return 0xC;
    case 0x31:
        return 0xD;
    case 0x32:
        return 0xE;
    case 0x29:
        return 0xF;
    default:
        return 0xFF;
    }
}

static uint8_t wmbus_get_bits_msb(const uint8_t* data, size_t bit_pos, size_t bit_count) {
    uint8_t out = 0;
    for(size_t i = 0; i < bit_count; i++) {
        size_t pos = bit_pos + i;
        uint8_t byte = data[pos / 8U];
        uint8_t bit = 7U - (pos % 8U);
        out = (out << 1) | ((byte >> bit) & 0x01U);
    }
    return out;
}

static bool wmbus_decode_3of6(
    const uint8_t* raw,
    size_t raw_len,
    uint8_t* out,
    size_t out_max,
    size_t* out_len) {
    size_t bit_len = raw_len * 8U;
    size_t bit_pos = 0;
    size_t out_idx = 0;
    bool have_high = false;
    uint8_t high_nibble = 0;

    while((bit_pos + 6U) <= bit_len) {
        uint8_t sym = wmbus_get_bits_msb(raw, bit_pos, 6);
        bit_pos += 6U;

        uint8_t nibble = wmbus_3of6_decode_symbol(sym);
        if(nibble == 0xFF) {
            return false;
        }

        if(!have_high) {
            high_nibble = nibble;
            have_high = true;
        } else {
            if(out_idx >= out_max) break;
            out[out_idx++] = (high_nibble << 4) | nibble;
            have_high = false;
        }
    }

    *out_len = out_idx;
    return out_idx > 0;
}

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

static size_t wmbus_frame_len_format_a(uint8_t l_field) {
    size_t n = l_field;
    size_t len = 1U + n + 2U; // L + data + first CRC
    if(n > 9U) {
        size_t rem = n - 9U;
        size_t blocks = (rem + 15U) / 16U;
        len += 2U * blocks;
    }
    return len;
}

static size_t wmbus_frame_len_format_b(uint8_t l_field) {
    return 1U + (size_t)l_field;
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
        size_t len_a = wmbus_frame_len_format_a(l_field);
        if(len_a <= len) return wmbus_crc_check_frame_a(data, len_a);
    } else {
        size_t len_b = wmbus_frame_len_format_b(l_field);
        if(len_b <= len) return wmbus_crc_check_frame_b(data, len_b);
    }
    return false;
}

static bool wmbus_mfg_valid(uint16_t man) {
    uint8_t a = (man >> 10) & 0x1F;
    uint8_t b = (man >> 5) & 0x1F;
    uint8_t c = man & 0x1F;
    return (a >= 1 && a <= 26) && (b >= 1 && b <= 26) && (c >= 1 && c <= 26);
}

static bool wmbus_is_plausible(const uint8_t* data, size_t len) {
    if(len < 11) return false;
    uint8_t l_field = data[0];
    if(l_field < 10) return false;
    uint16_t man = (uint16_t)data[2] | ((uint16_t)data[3] << 8);
    if(!wmbus_mfg_valid(man)) return false;
    return true;
}

static bool wmbus_l_field_valid(uint8_t l_field) {
    return l_field >= 10;
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
    bool crc_ok) {
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

    model->length_ok = length_ok;
    wmbus_history_push(model, frame, frame_len, used_3of6, rssi, crc_ok);
}

static bool wmbus_navigation_callback(void* context) {
    furi_assert(context);
    WmBusApp* app = context;
    view_dispatcher_stop(app->view_dispatcher);
    return true;
}

static int32_t wmbus_rx_thread(void* context) {
    WmBusApp* app = context;

    if(!furi_hal_subghz_is_frequency_valid(WMBUS_FREQ_HZ)) {
        with_view_model(app->view, WmBusViewModel * model, { model->freq_valid = false; }, true);
        return 0;
    }

    furi_hal_power_suppress_charge_enter();

    uint8_t sync_index = 0;
    wmbus_radio_apply(sync_index);

    uint8_t raw[WMBUS_RAW_MAX];
    uint8_t decoded[WMBUS_DECODE_MAX];
    uint8_t temp[WMBUS_FIFO_CHUNK];
    size_t raw_len = 0;
    bool in_packet = false;
    size_t expected_raw_len = 0;
    size_t expected_decoded_len = 0;
    uint32_t last_byte_tick = 0;
    uint32_t gap_ticks = furi_kernel_get_tick_frequency() / 200;
    if(gap_ticks < 1) gap_ticks = 1;

    while(app->running) {
        bool had_data = false;
        bool force_complete = false;

        while(furi_hal_subghz_rx_pipe_not_empty()) {
            uint8_t chunk_len = 0;
            furi_hal_subghz_read_packet(temp, &chunk_len);
            if(chunk_len == 0) break;

            size_t copy = WMBUS_RAW_MAX - raw_len;
            if(copy > 0) {
                if(chunk_len < copy) copy = chunk_len;
                memcpy(&raw[raw_len], temp, copy);
                raw_len += copy;
            }

            had_data = true;
            in_packet = true;
            last_byte_tick = furi_get_tick();
            if(raw_len >= WMBUS_RAW_MAX) break;

            if(expected_raw_len == 0) {
                if(sync_index == 1) {
                    if(raw_len >= 1) {
                        uint8_t l_field = raw[0];
                        if(wmbus_l_field_valid(l_field)) {
                            expected_raw_len = wmbus_frame_len_format_b(l_field);
                        }
                    }
                } else {
                    size_t tmp_len = 0;
                    if(raw_len >= 2 &&
                       wmbus_decode_3of6(raw, raw_len, decoded, sizeof(decoded), &tmp_len) &&
                       tmp_len >= 1) {
                        uint8_t l_field = decoded[0];
                        if(wmbus_l_field_valid(l_field)) {
                            expected_decoded_len = wmbus_frame_len_format_a(l_field);
                            expected_raw_len = (expected_decoded_len * 12U + 7U) / 8U;
                        }
                    }
                }
                if(expected_raw_len > WMBUS_RAW_MAX) expected_raw_len = WMBUS_RAW_MAX;
            }

            if(expected_raw_len > 0 && raw_len >= expected_raw_len) {
                force_complete = true;
                break;
            }
        }

        if(in_packet) {
            uint32_t now = furi_get_tick();
            bool gap = force_complete || (!had_data && (now - last_byte_tick) >= gap_ticks);
            bool full = (raw_len >= WMBUS_RAW_MAX);

            if((gap || full) && raw_len > 0) {
                size_t frame_raw_len = raw_len;
                if(expected_raw_len > 0 && raw_len > expected_raw_len) {
                    frame_raw_len = expected_raw_len;
                }

                int rssi = (int)furi_hal_subghz_get_rssi();

                size_t decoded_len = 0;
                bool decoded_ok = false;
                bool plausible = false;
                const uint8_t* frame = NULL;
                size_t frame_len = 0;
                bool used_3of6 = false;
                bool length_ok = false;
                bool crc_ok = false;
                bool crc_known = false;

                if(sync_index == 0) {
                    decoded_ok =
                        wmbus_decode_3of6(raw, frame_raw_len, decoded, sizeof(decoded), &decoded_len);
                    if(decoded_ok && wmbus_is_plausible(decoded, decoded_len)) {
                        frame = decoded;
                        frame_len = decoded_len;
                        used_3of6 = true;
                        plausible = true;
                    }
                } else {
                    decoded_ok = true;
                    if(wmbus_is_plausible(raw, frame_raw_len)) {
                        frame = raw;
                        frame_len = frame_raw_len;
                        plausible = true;
                    }
                }

                if(plausible) {
                    uint8_t l_field = frame[0];
                    size_t expected_len =
                        used_3of6 ? wmbus_frame_len_format_a(l_field) :
                                    wmbus_frame_len_format_b(l_field);
                    length_ok = (frame_len >= expected_len);
                    if(length_ok) {
                        crc_ok = wmbus_crc_ok(frame, frame_len, used_3of6);
                        crc_known = true;
                    }
                }

                bool strong_rssi = (rssi >= WMBUS_RSSI_STRONG_DBM);
                WmBusStatus status = WmBusStatusNone;
                if(sync_index == 0 && !decoded_ok) {
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

                with_view_model(
                    app->view,
                    WmBusViewModel * model,
                    {
                        model->packets_seen++;
                        model->last_raw_len = (uint16_t)frame_raw_len;
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
                                (uint8_t)frame_raw_len,
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
                            model->packets_per_sec =
                                (uint16_t)((diff * tick_freq) / (elapsed ? elapsed : 1U));
                            model->rate_last_tick = now_tick;
                            model->rate_last_seen = model->packets_seen;
                        }

                        wmbus_rssi_hist_push(model, rssi);
                    },
                    true);

                raw_len = 0;
                in_packet = false;
                expected_raw_len = 0;
                expected_decoded_len = 0;
                furi_hal_subghz_flush_rx();
                // SFRX leaves CC1101 in IDLE; resume RX to keep listening.
                furi_hal_subghz_rx();
            }
        }

        if(app->mode_change) {
            app->mode_change = false;
            if(app->mode_request < (uint8_t)(sizeof(wmbus_sync_list) / sizeof(wmbus_sync_list[0]))) {
                sync_index = app->mode_request;
                wmbus_radio_apply(sync_index);
                raw_len = 0;
                in_packet = false;
                expected_raw_len = 0;
                expected_decoded_len = 0;
                with_view_model(
                    app->view,
                    WmBusViewModel * model,
                    { model->sync_index = sync_index; },
                    true);
            }
        }

        if(!had_data) {
            furi_delay_ms(1);
        }
    }

    furi_hal_subghz_sleep();
    furi_hal_power_suppress_charge_exit();
    return 0;
}

static WmBusApp* wmbus_app_alloc(void) {
    WmBusApp* app = malloc(sizeof(WmBusApp));

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
    app->view_ctx.mode_change = &app->mode_change;
    app->view_ctx.mode_request = &app->mode_request;
    wmbus_view_setup(app->view, &app->view_ctx);
    view_dispatcher_add_view(app->view_dispatcher, 0, app->view);

    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, wmbus_navigation_callback);

    app->running = true;
    app->mode_change = false;
    app->mode_request = 0;
    app->rx_thread = furi_thread_alloc_ex("WmBusRx", 4096, wmbus_rx_thread, app);
    furi_thread_start(app->rx_thread);

    return app;
}

static void wmbus_app_free(WmBusApp* app) {
    app->running = false;
    if(app->rx_thread) {
        furi_thread_join(app->rx_thread);
        furi_thread_free(app->rx_thread);
    }

    view_dispatcher_remove_view(app->view_dispatcher, 0);
    view_dispatcher_free(app->view_dispatcher);
    view_free(app->view);
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
