#include "wmbus_app_i.h"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_spi.h>

#include <gui/view.h>

#include <lib/drivers/cc1101_regs.h>

#include "../protocol/wmbus_packet.h"
#include "../protocol/capture/wmbus_capture.h"
#include "../storage/wmbus_log.h"

#define TAG "WmBusDecoder"

#define WMBUS_FREQ_HZ           868950000UL
#define WMBUS_FIFO_CHUNK        64U
#define WMBUS_RSSI_UPDATE_HZ    5U
#define WMBUS_LED_PULSE_MS      40U
#define WMBUS_T_GAP_TIMEOUT_MS  5U
#define WMBUS_C_READ_TIMEOUT_MS 25U
#define WMBUS_MODE_COUNT        2U

typedef enum {
    WmBusControlCmdStop = 0,
    WmBusControlCmdApplyConfig,
} WmBusControlCmd;

typedef struct {
    WmBusControlCmd cmd;
    WmBusSettings settings;
} WmBusControlEvent;

static uint8_t wmbus_cc1101_preset_regs[];
static void wmbus_app_log_step(const char* step);
static void wmbus_radio_recover_rx(void);
static bool wmbus_radio_validate_c_mode_regs(void);
static void wmbus_radio_reload_rx_preset(void);
static void wmbus_app_copy_keyring(WmBusApp* app, WmBusKeyring* keyring);

static void wmbus_app_log_step(const char* step) {
    if(step) {
        FURI_LOG_D(TAG, "app init: %s", step);
    }
}

static bool wmbus_radio_preset_loadable(void) {
    for(size_t i = 0;; i += 2U) {
        uint8_t reg = wmbus_cc1101_preset_regs[i];
        uint8_t value = wmbus_cc1101_preset_regs[i + 1U];

        if(reg == 0U && value == 0U) {
            return true;
        }

        if(reg == 0U) {
            return false;
        }
    }
}

static void wmbus_preset_set_reg(uint8_t reg, uint8_t value) {
    for(size_t i = 0;; i += 2U) {
        if(wmbus_cc1101_preset_regs[i] == 0U && wmbus_cc1101_preset_regs[i + 1U] == 0U) break;
        if(wmbus_cc1101_preset_regs[i] == reg) {
            wmbus_cc1101_preset_regs[i + 1U] = value;
            return;
        }
    }
}

static uint8_t wmbus_radio_read_reg(uint8_t reg) {
    uint8_t cmd[2] = {(uint8_t)(reg | CC1101_READ), 0U};
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz);
    furi_hal_spi_bus_trx(&furi_hal_spi_bus_handle_subghz, cmd, cmd, sizeof(cmd), CC1101_TIMEOUT);
    furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz);
    return cmd[1];
}

static void wmbus_radio_apply_t_mode(void) {
    wmbus_preset_set_reg(CC1101_SYNC1, 0x54);
    wmbus_preset_set_reg(CC1101_SYNC0, 0x3D);
    wmbus_preset_set_reg(CC1101_IOCFG0, 0x00);
    wmbus_preset_set_reg(CC1101_MDMCFG2, 0x05);
    wmbus_preset_set_reg(CC1101_MCSM1, 0x00);
    wmbus_preset_set_reg(CC1101_PKTCTRL1, 0x00);
    wmbus_preset_set_reg(CC1101_PKTCTRL0, 0x02);
}

static void wmbus_radio_apply_c_mode(void) {
    wmbus_preset_set_reg(CC1101_SYNC1, 0x54);
    wmbus_preset_set_reg(CC1101_SYNC0, 0x3D);
    wmbus_preset_set_reg(CC1101_IOCFG0, 0x06);
    wmbus_preset_set_reg(CC1101_MCSM1, 0x00);
    wmbus_preset_set_reg(CC1101_MDMCFG2, 0x01);
    wmbus_preset_set_reg(CC1101_PKTCTRL1, 0x00);
    wmbus_preset_set_reg(CC1101_PKTCTRL0, 0x42);
}

static void wmbus_radio_apply_mode(WmBusRxMode mode) {
    furi_check(wmbus_radio_preset_loadable());

    if(mode == WmBusRxModeC) {
        wmbus_radio_apply_c_mode();
    } else {
        wmbus_radio_apply_t_mode();
    }

    furi_hal_subghz_reset();
    furi_hal_subghz_load_custom_preset(wmbus_cc1101_preset_regs);
    furi_hal_subghz_set_frequency_and_path(WMBUS_FREQ_HZ);
    furi_hal_subghz_flush_rx();
    furi_hal_subghz_rx();

    if(mode == WmBusRxModeC && !wmbus_radio_validate_c_mode_regs()) {
        wmbus_radio_reload_rx_preset();
        if(!wmbus_radio_validate_c_mode_regs()) {
            FURI_LOG_W(TAG, "C cfg still mismatched after preset reload");
            wmbus_radio_recover_rx();
        }
    }
}

static void wmbus_radio_recover_rx(void) {
    furi_hal_subghz_idle();
    furi_hal_subghz_flush_rx();
    furi_hal_subghz_rx();
}

static void wmbus_radio_reload_rx_preset(void) {
    furi_hal_subghz_reset();
    furi_hal_subghz_load_custom_preset(wmbus_cc1101_preset_regs);
    furi_hal_subghz_set_frequency_and_path(WMBUS_FREQ_HZ);
    furi_hal_subghz_flush_rx();
    furi_hal_subghz_rx();
}

static bool wmbus_radio_validate_c_mode_regs(void) {
    uint8_t pktctrl0 = wmbus_radio_read_reg(CC1101_PKTCTRL0);
    uint8_t pktctrl1 = wmbus_radio_read_reg(CC1101_PKTCTRL1);
    uint8_t mdmcfg2 = wmbus_radio_read_reg(CC1101_MDMCFG2);
    uint8_t sync1 = wmbus_radio_read_reg(CC1101_SYNC1);
    uint8_t sync0 = wmbus_radio_read_reg(CC1101_SYNC0);

    FURI_LOG_D(
        TAG,
        "C cfg SYNC=%02X%02X PKTCTRL0=%02X PKTCTRL1=%02X MDMCFG2=%02X",
        sync1,
        sync0,
        pktctrl0,
        pktctrl1,
        mdmcfg2);

    bool ok = (pktctrl0 == 0x42U) && (pktctrl1 == 0x00U) && (mdmcfg2 == 0x01U) &&
              (sync1 == 0x54U) && (sync0 == 0x3DU);
    if(!ok) {
        FURI_LOG_W(
            TAG,
            "C cfg mismatch (SYNC=%02X%02X PKTCTRL0=%02X PKTCTRL1=%02X MDMCFG2=%02X)",
            sync1,
            sync0,
            pktctrl0,
            pktctrl1,
            mdmcfg2);
    }
    return ok;
}

static uint8_t wmbus_radio_read_fifo_raw(uint8_t* data, uint8_t data_max, bool* overflowed) {
    if(!data || data_max == 0U) return 0U;

    uint8_t size = 0U;
    bool overflow = false;
    uint8_t rxbytes_cmd[2] = {CC1101_STATUS_RXBYTES | CC1101_READ | CC1101_BURST, 0U};

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

    if(!overflow && size > 0U) {
        uint8_t addr = CC1101_FIFO | CC1101_READ | CC1101_BURST;
        furi_hal_spi_bus_tx(&furi_hal_spi_bus_handle_subghz, &addr, 1U, CC1101_TIMEOUT);
        furi_hal_spi_bus_rx(&furi_hal_spi_bus_handle_subghz, data, size, CC1101_TIMEOUT);
    }

    furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz);

    if(overflow) {
        wmbus_radio_recover_rx();
        size = 0U;
    }
    if(overflowed) *overflowed = overflow;
    return size;
}

static uint8_t wmbus_cc1101_preset_regs[] = {
    // IOCFG2 is register 0x00 and cannot appear in this table because the
    // Flipper preset loader uses 0x00 as the end-of-table sentinel.
    CC1101_IOCFG1,
    0x2E,
    CC1101_IOCFG0,
    0x00,
    CC1101_FIFOTHR,
    0x07,
    CC1101_SYNC1,
    0x54,
    CC1101_SYNC0,
    0x3D,
    CC1101_PKTLEN,
    0xFF,
    CC1101_PKTCTRL1,
    0x04,
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
    CC1101_FREQ2,
    0x21,
    CC1101_FREQ1,
    0x65,
    CC1101_FREQ0,
    0x6A,
    CC1101_MDMCFG4,
    0x5C,
    CC1101_MDMCFG3,
    0x04,
    CC1101_MDMCFG2,
    0x05,
    CC1101_MDMCFG1,
    0x22,
    CC1101_MDMCFG0,
    0xF8,
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
    0U,
    0U,
    0xC0,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
};

static bool wmbus_capture_t_step(
    WmBusCaptureStateT* state,
    WmBusCaptureFrame* frame,
    bool* had_data,
    uint32_t gap_ticks) {
    if(!state || !frame || !had_data) return false;

    bool force_complete = false;
    uint8_t temp[WMBUS_FIFO_CHUNK];

    while(true) {
        bool overflow = false;
        uint8_t chunk_len = wmbus_radio_read_fifo_raw(temp, sizeof(temp), &overflow);
        if(overflow) {
            wmbus_capture_state_t_reset(state);
            return false;
        }
        if(chunk_len == 0U) break;

        size_t copy = sizeof(state->raw) - state->raw_len;
        if(copy > 0U) {
            if(chunk_len < copy) copy = chunk_len;
            memcpy(&state->raw[state->raw_len], temp, copy);
            state->raw_len += copy;
        }

        *had_data = true;
        state->in_packet = true;
        state->last_byte_tick = furi_get_tick();
        if(state->raw_len >= sizeof(state->raw)) break;

        if(state->expected_raw_len == 0U) {
            size_t expected_raw_len = 0U;
            if(wmbus_capture_estimate_t_expected_raw_len(
                   state->raw, state->raw_len, sizeof(state->raw), &expected_raw_len)) {
                state->expected_raw_len = expected_raw_len;
            }
        }

        if(state->expected_raw_len > 0U && state->raw_len >= state->expected_raw_len) {
            force_complete = true;
            break;
        }
    }

    if(state->in_packet) {
        uint32_t now = furi_get_tick();
        bool gap = force_complete || (!*had_data && (now - state->last_byte_tick) >= gap_ticks);
        bool full = (state->raw_len >= sizeof(state->raw));

        if((gap || full) && state->raw_len > 0U) {
            size_t frame_raw_len = state->raw_len;
            if(state->expected_raw_len > 0U && frame_raw_len > state->expected_raw_len) {
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
    bool* had_data,
    uint32_t gap_ticks) {
    if(!state || !frame || !had_data) return false;

    bool force_complete = false;
    uint8_t temp[WMBUS_FIFO_CHUNK];

    while(true) {
        bool overflow = false;
        uint8_t chunk_len = wmbus_radio_read_fifo_raw(temp, sizeof(temp), &overflow);
        if(overflow) {
            wmbus_capture_state_c_reset(state);
            return false;
        }
        if(chunk_len == 0U) break;

        size_t copy = sizeof(state->raw) - state->raw_len;
        if(copy > 0U) {
            if(chunk_len < copy) copy = chunk_len;
            memcpy(&state->raw[state->raw_len], temp, copy);
            state->raw_len += copy;
        }

        *had_data = true;
        state->in_packet = true;
        state->last_byte_tick = furi_get_tick();
        if(state->raw_len >= sizeof(state->raw)) break;

        if(state->expected_len == 0U) {
            size_t expected_len = 0U;
            if(wmbus_capture_estimate_c_expected_len(
                   state->raw, state->raw_len, sizeof(state->raw), &expected_len)) {
                state->expected_len = expected_len;
            }
        }

        if(state->expected_len > 0U && state->raw_len >= state->expected_len) {
            force_complete = true;
            break;
        }
    }

    if(!state->in_packet) return false;

    uint32_t now = furi_get_tick();
    bool gap = force_complete || (!*had_data && (now - state->last_byte_tick) >= gap_ticks);
    bool full = (state->raw_len >= sizeof(state->raw));
    if(!(gap || full) || state->raw_len == 0U) return false;

    size_t frame_len = state->raw_len;
    if(state->expected_len > 0U && frame_len > state->expected_len) {
        frame_len = state->expected_len;
    }

    size_t frame_offset = wmbus_capture_c_frame_offset(state->raw, frame_len);
    if(frame_offset == SIZE_MAX || frame_len <= frame_offset) {
        wmbus_capture_state_c_reset(state);
        wmbus_radio_recover_rx();
        return false;
    }

    memcpy(frame->data, &state->raw[frame_offset], frame_len - frame_offset);
    frame->len = frame_len - frame_offset;
    frame->raw_len = frame_len;
    frame->rssi = (int)furi_hal_subghz_get_rssi();
    frame->mode = WmBusRxModeC;

    wmbus_capture_state_c_reset(state);
    wmbus_radio_recover_rx();
    return true;
}

static void wmbus_process_captured_frame(
    WmBusApp* app,
    const WmBusSettings* settings,
    const WmBusKeyring* keyring,
    const WmBusCaptureFrame* capture) {
    if(!app || !settings || !capture) return;

    WmBusPacketRecord record = {0};
    if(!wmbus_packet_process_capture(capture, keyring, &record)) {
        return;
    }

    if((settings->csv_logging != WmBusCsvLoggingNone) &&
       wmbus_status_meets_threshold(record.status, settings->csv_threshold)) {
        wmbus_log_append(app->storage, settings->csv_logging, &record);
    }

    bool store_in_history =
        wmbus_status_meets_threshold(record.status, settings->memory_threshold);
    wmbus_rx_view_push_packet(app->rx_view, &record, store_in_history);
}

static void wmbus_app_copy_keyring(WmBusApp* app, WmBusKeyring* keyring) {
    if(!app || !keyring || !app->keyring_mutex) return;

    furi_check(furi_mutex_acquire(app->keyring_mutex, FuriWaitForever) == FuriStatusOk);
    *keyring = app->keyring;
    furi_check(furi_mutex_release(app->keyring_mutex) == FuriStatusOk);
}

static int32_t wmbus_rx_thread(void* context) {
    WmBusApp* app = context;

    if(!furi_hal_subghz_is_frequency_valid(WMBUS_FREQ_HZ)) {
        FURI_LOG_W(TAG, "frequency %lu invalid", (unsigned long)WMBUS_FREQ_HZ);
        wmbus_rx_view_set_freq_valid(app->rx_view, false);
        return 0;
    }

    furi_hal_power_suppress_charge_enter();

    WmBusSettings runtime_settings = app->settings;
    WmBusKeyring runtime_keyring;
    wmbus_app_copy_keyring(app, &runtime_keyring);
    WmBusRxMode mode = runtime_settings.mode;
    wmbus_radio_apply_mode(mode);

    WmBusCaptureStateT capture_t = {0};
    WmBusCaptureStateC capture_c = {0};
    wmbus_capture_state_t_reset(&capture_t);
    wmbus_capture_state_c_reset(&capture_c);

    uint32_t tick_freq = furi_kernel_get_tick_frequency();
    uint32_t t_gap_ticks = (tick_freq * WMBUS_T_GAP_TIMEOUT_MS + 999U) / 1000U;
    uint32_t c_gap_ticks = (tick_freq * WMBUS_C_READ_TIMEOUT_MS + 999U) / 1000U;
    uint32_t rssi_ticks = tick_freq / WMBUS_RSSI_UPDATE_HZ;
    uint32_t led_pulse_ticks = (tick_freq * WMBUS_LED_PULSE_MS + 999U) / 1000U;
    if(t_gap_ticks < 1U) t_gap_ticks = 1U;
    if(c_gap_ticks < 1U) c_gap_ticks = 1U;
    if(rssi_ticks < 1U) rssi_ticks = 1U;
    if(led_pulse_ticks < 1U) led_pulse_ticks = 1U;

    uint32_t last_rssi_tick = 0U;
    uint32_t led_pulse_off_tick = 0U;
    bool led_pulse_on = false;
    bool running = true;

    while(running) {
        WmBusControlEvent event;
        while(furi_message_queue_get(app->control_queue, &event, 0U) == FuriStatusOk) {
            if(event.cmd == WmBusControlCmdStop) {
                running = false;
                break;
            }

            if(event.cmd == WmBusControlCmdApplyConfig) {
                bool mode_changed = (runtime_settings.mode != event.settings.mode);
                runtime_settings = event.settings;
                wmbus_app_copy_keyring(app, &runtime_keyring);
                if(mode_changed) {
                    mode = runtime_settings.mode;
                    wmbus_radio_apply_mode(mode);
                    wmbus_capture_state_t_reset(&capture_t);
                    wmbus_capture_state_c_reset(&capture_c);
                }
            }
        }

        if(!running) break;

        bool had_data = false;
        WmBusCaptureFrame capture = {0};
        bool frame_ready = false;

        if(mode == WmBusRxModeT) {
            frame_ready = wmbus_capture_t_step(&capture_t, &capture, &had_data, t_gap_ticks);
        } else {
            frame_ready = wmbus_capture_c_step(&capture_c, &capture, &had_data, c_gap_ticks);
        }

        if(frame_ready) {
            uint32_t pulse_now = furi_get_tick();
            furi_hal_light_set(LightGreen, 0xFF);
            led_pulse_on = true;
            led_pulse_off_tick = pulse_now + led_pulse_ticks;
            wmbus_process_captured_frame(app, &runtime_settings, &runtime_keyring, &capture);
        }

        uint32_t now_tick = furi_get_tick();
        if(led_pulse_on && ((int32_t)(now_tick - led_pulse_off_tick) >= 0)) {
            furi_hal_light_set(LightGreen, 0x00);
            led_pulse_on = false;
        }

        if(last_rssi_tick == 0U || (now_tick - last_rssi_tick) >= rssi_ticks) {
            wmbus_rx_view_set_live_rssi(app->rx_view, (int)furi_hal_subghz_get_rssi());
            last_rssi_tick = now_tick;
        }

        if(!had_data) {
            furi_delay_ms(1U);
        }
    }

    furi_hal_light_set(LightGreen, 0x00);
    furi_hal_subghz_sleep();
    furi_hal_power_suppress_charge_exit();
    return 0;
}

static bool wmbus_app_custom_event_callback(void* context, uint32_t event) {
    WmBusApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool wmbus_app_back_event_callback(void* context) {
    WmBusApp* app = context;
    if(!scene_manager_handle_back_event(app->scene_manager)) {
        view_dispatcher_stop(app->view_dispatcher);
    }
    return true;
}

bool wmbus_app_apply_runtime_config(WmBusApp* app, bool persist) {
    if(!app) return false;

    wmbus_rx_view_apply_settings(app->rx_view, &app->settings);
    if(persist) {
        wmbus_settings_save(app->storage, &app->settings);
    }

    if(app->control_queue) {
        WmBusControlEvent event = {
            .cmd = WmBusControlCmdApplyConfig,
            .settings = app->settings,
        };
        return furi_message_queue_put(app->control_queue, &event, FuriWaitForever) == FuriStatusOk;
    }

    return true;
}

bool wmbus_app_reload_keys(WmBusApp* app) {
    if(!app) return false;
    furi_check(furi_mutex_acquire(app->keyring_mutex, FuriWaitForever) == FuriStatusOk);
    bool loaded = wmbus_keyring_load(app->storage, &app->keyring);
    furi_check(furi_mutex_release(app->keyring_mutex) == FuriStatusOk);
    wmbus_app_apply_runtime_config(app, false);
    return loaded;
}

bool wmbus_app_add_key(WmBusApp* app, const uint8_t key[WMBUS_KEY_BYTES]) {
    if(!app || !key) return false;
    furi_check(furi_mutex_acquire(app->keyring_mutex, FuriWaitForever) == FuriStatusOk);
    bool added = wmbus_keyring_append(app->storage, &app->keyring, key);
    furi_check(furi_mutex_release(app->keyring_mutex) == FuriStatusOk);
    if(added) {
        wmbus_app_apply_runtime_config(app, false);
    }
    return added;
}

bool wmbus_app_build_detail_text(WmBusApp* app, char* out, size_t out_size) {
    if(!app) return false;
    return wmbus_rx_view_build_selected_detail_text(app->rx_view, out, out_size);
}

bool wmbus_app_ensure_config_view(WmBusApp* app) {
    if(!app) return false;
    if(app->config_list) return true;

    wmbus_app_log_step("alloc config view");
    app->config_list = variable_item_list_alloc();
    if(!app->config_list) return false;

    view_dispatcher_add_view(
        app->view_dispatcher, WmBusAppViewConfig, variable_item_list_get_view(app->config_list));
    return true;
}

bool wmbus_app_ensure_key_input_view(WmBusApp* app) {
    if(!app) return false;
    if(app->key_input) return true;

    wmbus_app_log_step("alloc key input view");
    app->key_input = byte_input_alloc();
    if(!app->key_input) return false;

    view_dispatcher_add_view(
        app->view_dispatcher, WmBusAppViewKeyInput, byte_input_get_view(app->key_input));
    return true;
}

bool wmbus_app_ensure_status_mask_view(WmBusApp* app) {
    if(!app) return false;
    if(app->status_mask_list) return true;

    wmbus_app_log_step("alloc status mask view");
    app->status_mask_list = variable_item_list_alloc();
    if(!app->status_mask_list) return false;

    view_dispatcher_add_view(
        app->view_dispatcher,
        WmBusAppViewStatusMask,
        variable_item_list_get_view(app->status_mask_list));
    return true;
}

bool wmbus_app_ensure_detail_view(WmBusApp* app) {
    if(!app) return false;
    if(app->detail_widget) return true;

    wmbus_app_log_step("alloc detail view");
    app->detail_widget = widget_alloc();
    if(!app->detail_widget) return false;

    view_dispatcher_add_view(
        app->view_dispatcher, WmBusAppViewPacketDetail, widget_get_view(app->detail_widget));
    return true;
}

static WmBusApp* wmbus_app_alloc(void) {
    WmBusApp* app = malloc(sizeof(WmBusApp));
    memset(app, 0, sizeof(*app));

    wmbus_app_log_step("open records");
    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);

    wmbus_app_log_step("load settings");
    wmbus_settings_reset(&app->settings);
    wmbus_settings_load(app->storage, &app->settings);
    wmbus_app_log_step("load keyring");
    wmbus_keyring_init(&app->keyring);
    wmbus_keyring_load(app->storage, &app->keyring);

    wmbus_app_log_step("alloc core");
    app->keyring_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->control_queue = furi_message_queue_alloc(4U, sizeof(WmBusControlEvent));
    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&wmbus_scene_handlers, app);
    wmbus_app_log_step("alloc rx view");
    app->rx_view = wmbus_rx_view_alloc();

    wmbus_rx_view_set_dispatcher(app->rx_view, app->view_dispatcher);
    wmbus_rx_view_apply_settings(app->rx_view, &app->settings);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, wmbus_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, wmbus_app_back_event_callback);

    wmbus_app_log_step("add rx view");
    view_dispatcher_add_view(
        app->view_dispatcher, WmBusAppViewRx, wmbus_rx_view_get_view(app->rx_view));

    wmbus_app_log_step("attach gui");
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    wmbus_app_log_step("enter rx scene");
    scene_manager_next_scene(app->scene_manager, WmBusSceneRx);
    wmbus_app_log_step("start rx thread");
    app->rx_thread = furi_thread_alloc_ex("WmBusRx", 8192U, wmbus_rx_thread, app);
    furi_thread_start(app->rx_thread);
    wmbus_app_log_step("init done");
    return app;
}

static void wmbus_app_free(WmBusApp* app) {
    if(!app) return;

    if(app->control_queue) {
        WmBusControlEvent event = {.cmd = WmBusControlCmdStop};
        furi_message_queue_put(app->control_queue, &event, FuriWaitForever);
    }

    if(app->rx_thread) {
        furi_thread_join(app->rx_thread);
        furi_thread_free(app->rx_thread);
    }

    if(app->detail_widget) {
        view_dispatcher_remove_view(app->view_dispatcher, WmBusAppViewPacketDetail);
        widget_free(app->detail_widget);
    }
    if(app->status_mask_list) {
        view_dispatcher_remove_view(app->view_dispatcher, WmBusAppViewStatusMask);
        variable_item_list_free(app->status_mask_list);
    }
    if(app->config_list) {
        view_dispatcher_remove_view(app->view_dispatcher, WmBusAppViewConfig);
        variable_item_list_free(app->config_list);
    }
    if(app->key_input) {
        view_dispatcher_remove_view(app->view_dispatcher, WmBusAppViewKeyInput);
        byte_input_free(app->key_input);
    }
    view_dispatcher_remove_view(app->view_dispatcher, WmBusAppViewRx);
    wmbus_rx_view_free(app->rx_view);
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_message_queue_free(app->control_queue);
    furi_mutex_free(app->keyring_mutex);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t wmbus_app(void) {
    WmBusApp* app = wmbus_app_alloc();
    view_dispatcher_run(app->view_dispatcher);
    wmbus_app_free(app);
    return 0;
}
