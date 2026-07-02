#include "wmbus_radio_rx_service.h"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_spi.h>
#include <string.h>

#include <lib/drivers/cc1101_regs.h>

#define TAG "WmBusDecoder"

#define WMBUS_FREQ_HZ           868950000UL
#define WMBUS_FIFO_CHUNK        64U
#define WMBUS_RSSI_UPDATE_HZ    5U
#define WMBUS_LED_PULSE_MS      40U
#define WMBUS_T_GAP_TIMEOUT_MS  5U
#define WMBUS_C_READ_TIMEOUT_MS 25U

typedef enum {
    WmBusControlCmdStop = 0,
    WmBusControlCmdApplyConfig,
} WmBusControlCmd;

typedef struct {
    WmBusControlCmd cmd;
    WmBusSettings settings;
    WmBusCryptoKeyStore key_store;
} WmBusControlEvent;

struct WmBusRadioRxService {
    WmBusRadioRxCallbacks callbacks;
    FuriThread* thread;
    FuriMessageQueue* control_queue;
    WmBusSettings settings;
    WmBusCryptoKeyStore key_store;
};

static uint8_t wmbus_cc1101_preset_regs[];

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
    uint8_t iocfg0 = wmbus_radio_read_reg(CC1101_IOCFG0);
    uint8_t pktctrl0 = wmbus_radio_read_reg(CC1101_PKTCTRL0);
    uint8_t pktctrl1 = wmbus_radio_read_reg(CC1101_PKTCTRL1);
    uint8_t mdmcfg2 = wmbus_radio_read_reg(CC1101_MDMCFG2);
    uint8_t sync1 = wmbus_radio_read_reg(CC1101_SYNC1);
    uint8_t sync0 = wmbus_radio_read_reg(CC1101_SYNC0);

    FURI_LOG_D(
        TAG,
        "C cfg SYNC=%02X%02X IOCFG0=%02X PKTCTRL0=%02X PKTCTRL1=%02X MDMCFG2=%02X",
        sync1,
        sync0,
        iocfg0,
        pktctrl0,
        pktctrl1,
        mdmcfg2);

    bool ok = (iocfg0 == 0x00U) && (pktctrl0 == 0x02U) && (pktctrl1 == 0x00U) &&
              (mdmcfg2 == 0x05U) &&
              (sync1 == 0x54U) && (sync0 == 0x3DU);
    if(!ok) {
        FURI_LOG_W(
            TAG,
            "C cfg mismatch (SYNC=%02X%02X IOCFG0=%02X PKTCTRL0=%02X PKTCTRL1=%02X MDMCFG2=%02X)",
            sync1,
            sync0,
            iocfg0,
            pktctrl0,
            pktctrl1,
            mdmcfg2);
    }
    return ok;
}

static void wmbus_radio_apply_link_b_rx_base(void) {
    wmbus_preset_set_reg(CC1101_SYNC1, 0x54);
    wmbus_preset_set_reg(CC1101_SYNC0, 0x3D);
    wmbus_preset_set_reg(CC1101_IOCFG0, 0x00);
    wmbus_preset_set_reg(CC1101_MDMCFG2, 0x05);
    wmbus_preset_set_reg(CC1101_MCSM1, 0x00);
    wmbus_preset_set_reg(CC1101_PKTCTRL1, 0x00);
    wmbus_preset_set_reg(CC1101_PKTCTRL0, 0x02);
}

static void wmbus_radio_apply_mode(WmBusRxMode mode) {
    furi_check(wmbus_radio_preset_loadable());

    wmbus_radio_apply_link_b_rx_base();
    wmbus_radio_reload_rx_preset();
    if(mode == WmBusRxModeC && !wmbus_radio_validate_c_mode_regs()) {
        wmbus_radio_reload_rx_preset();
        if(!wmbus_radio_validate_c_mode_regs()) {
            FURI_LOG_W(TAG, "C cfg still mismatched after preset reload");
            wmbus_radio_recover_rx();
        }
    }
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

static bool wmbus_capture_append_fifo(
    uint8_t* raw,
    size_t* raw_len,
    size_t raw_max,
    bool* had_data,
    bool* overflowed,
    uint32_t* last_byte_tick) {
    uint8_t temp[WMBUS_FIFO_CHUNK];
    bool overflow = false;
    uint8_t chunk_len = wmbus_radio_read_fifo_raw(temp, sizeof(temp), &overflow);
    if(overflowed) *overflowed = overflow;
    if(overflow || chunk_len == 0U) {
        return false;
    }

    size_t copy = raw_max - *raw_len;
    if(copy > 0U) {
        if(chunk_len < copy) copy = chunk_len;
        memcpy(&raw[*raw_len], temp, copy);
        *raw_len += copy;
    }

    *had_data = true;
    *last_byte_tick = furi_get_tick();
    return true;
}

static bool wmbus_capture_t_step(
    WmBusCaptureStateT* state,
    WmBusCaptureFrame* frame,
    bool* had_data,
    uint32_t gap_ticks) {
    if(!state || !frame || !had_data) return false;

    bool force_complete = false;
    while(true) {
        bool overflow = false;
        if(!wmbus_capture_append_fifo(
               state->raw,
               &state->raw_len,
               sizeof(state->raw),
               had_data,
               &overflow,
               &state->last_byte_tick)) {
            if(overflow) {
                wmbus_capture_state_t_reset(state);
            }
            break;
        }

        state->in_packet = true;
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
    while(true) {
        bool overflow = false;
        if(!wmbus_capture_append_fifo(
               state->raw,
               &state->raw_len,
               sizeof(state->raw),
               had_data,
               &overflow,
               &state->last_byte_tick)) {
            if(overflow) {
                wmbus_capture_state_c_reset(state);
            }
            break;
        }

        state->in_packet = true;
        if(state->raw_len >= sizeof(state->raw)) break;

        size_t expected_len = 0U;
        if(wmbus_capture_estimate_c_expected_len(
               state->raw, state->raw_len, sizeof(state->raw), &expected_len)) {
            state->expected_len = expected_len;
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

    size_t frame_offset = 0U;
    size_t frame_len = 0U;
    if(!wmbus_capture_select_c_frame(
           state->raw, state->raw_len, state->expected_len, &frame_offset, &frame_len)) {
        wmbus_capture_state_c_reset(state);
        wmbus_radio_recover_rx();
        return false;
    }

    memcpy(frame->data, &state->raw[frame_offset], frame_len - frame_offset);
    frame->len = frame_len - frame_offset;
    frame->rssi = (int)furi_hal_subghz_get_rssi();
    frame->mode = WmBusRxModeC;
    wmbus_capture_state_c_reset(state);
    wmbus_radio_recover_rx();
    return true;
}

static uint32_t wmbus_ticks_from_ms(uint32_t ms) {
    uint32_t tick_freq = furi_kernel_get_tick_frequency();
    uint32_t ticks = (tick_freq * ms + 999U) / 1000U;
    return ticks == 0U ? 1U : ticks;
}

static int32_t wmbus_radio_rx_service_thread(void* context) {
    WmBusRadioRxService* service = context;

    if(!furi_hal_subghz_is_frequency_valid(WMBUS_FREQ_HZ)) {
        FURI_LOG_W(TAG, "frequency %lu invalid", (unsigned long)WMBUS_FREQ_HZ);
        if(service->callbacks.set_freq_valid) {
            service->callbacks.set_freq_valid(service->callbacks.context, false);
        }
        return 0;
    }

    furi_hal_power_suppress_charge_enter();

    WmBusSettings runtime_settings = service->settings;
    WmBusCryptoKeyStore runtime_key_store = service->key_store;
    WmBusRxMode mode = runtime_settings.mode;
    wmbus_radio_apply_mode(mode);

    WmBusCaptureStateT capture_t = {0};
    WmBusCaptureStateC capture_c = {0};
    wmbus_capture_state_t_reset(&capture_t);
    wmbus_capture_state_c_reset(&capture_c);

    uint32_t t_gap_ticks = wmbus_ticks_from_ms(WMBUS_T_GAP_TIMEOUT_MS);
    uint32_t c_gap_ticks = wmbus_ticks_from_ms(WMBUS_C_READ_TIMEOUT_MS);
    uint32_t rssi_ticks = wmbus_ticks_from_ms(1000U / WMBUS_RSSI_UPDATE_HZ);
    uint32_t led_pulse_ticks = wmbus_ticks_from_ms(WMBUS_LED_PULSE_MS);

    uint32_t last_rssi_tick = 0U;
    uint32_t led_pulse_off_tick = 0U;
    bool led_pulse_on = false;
    bool running = true;

    while(running) {
        WmBusControlEvent event;
        while(furi_message_queue_get(service->control_queue, &event, 0U) == FuriStatusOk) {
            if(event.cmd == WmBusControlCmdStop) {
                running = false;
                break;
            }

            if(event.cmd == WmBusControlCmdApplyConfig) {
                bool mode_changed = (runtime_settings.mode != event.settings.mode);
                runtime_settings = event.settings;
                runtime_key_store = event.key_store;
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
        bool frame_ready = (mode == WmBusRxModeT) ?
                               wmbus_capture_t_step(&capture_t, &capture, &had_data, t_gap_ticks) :
                               wmbus_capture_c_step(&capture_c, &capture, &had_data, c_gap_ticks);

        if(frame_ready) {
            uint32_t pulse_now = furi_get_tick();
            furi_hal_light_set(LightGreen, 0xFF);
            led_pulse_on = true;
            led_pulse_off_tick = pulse_now + led_pulse_ticks;
            if(service->callbacks.handle_capture) {
                service->callbacks.handle_capture(
                    service->callbacks.context, &runtime_settings, &runtime_key_store, &capture);
            }
        }

        uint32_t now_tick = furi_get_tick();
        if(led_pulse_on && ((int32_t)(now_tick - led_pulse_off_tick) >= 0)) {
            furi_hal_light_set(LightGreen, 0x00);
            led_pulse_on = false;
        }

        if(last_rssi_tick == 0U || (now_tick - last_rssi_tick) >= rssi_ticks) {
            if(service->callbacks.set_live_rssi) {
                service->callbacks.set_live_rssi(
                    service->callbacks.context, (int)furi_hal_subghz_get_rssi());
            }
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

WmBusRadioRxService* wmbus_radio_rx_service_alloc(
    const WmBusRadioRxCallbacks* callbacks,
    const WmBusSettings* settings,
    const WmBusCryptoKeyStore* key_store) {
    if(!callbacks || !settings || !key_store) {
        return NULL;
    }

    WmBusRadioRxService* service = malloc(sizeof(*service));
    if(!service) {
        return NULL;
    }

    *service = (WmBusRadioRxService){
        .callbacks = *callbacks,
        .settings = *settings,
        .key_store = *key_store,
    };

    service->control_queue = furi_message_queue_alloc(4U, sizeof(WmBusControlEvent));
    if(!service->control_queue) {
        free(service);
        return NULL;
    }

    service->thread =
        furi_thread_alloc_ex("WmBusRx", 12288U, wmbus_radio_rx_service_thread, service);
    if(!service->thread) {
        furi_message_queue_free(service->control_queue);
        free(service);
        return NULL;
    }

    furi_thread_start(service->thread);
    return service;
}

void wmbus_radio_rx_service_free(WmBusRadioRxService* service) {
    if(!service) return;

    if(service->control_queue) {
        WmBusControlEvent event = {.cmd = WmBusControlCmdStop};
        furi_message_queue_put(service->control_queue, &event, FuriWaitForever);
    }

    if(service->thread) {
        furi_thread_join(service->thread);
        furi_thread_free(service->thread);
    }

    if(service->control_queue) {
        furi_message_queue_free(service->control_queue);
    }
    free(service);
}

bool wmbus_radio_rx_service_apply_config(
    WmBusRadioRxService* service,
    const WmBusSettings* settings,
    const WmBusCryptoKeyStore* key_store) {
    if(!service || !settings || !key_store || !service->control_queue) {
        return false;
    }

    WmBusControlEvent event = {
        .cmd = WmBusControlCmdApplyConfig,
        .settings = *settings,
        .key_store = *key_store,
    };
    return furi_message_queue_put(service->control_queue, &event, FuriWaitForever) ==
           FuriStatusOk;
}

static uint8_t wmbus_cc1101_preset_regs[] = {
    // IOCFG2 is register 0x00 and cannot appear in this table because the
    // Flipper preset loader uses 0x00 as the end-of-table sentinel.
    CC1101_IOCFG1, 0x2E, CC1101_IOCFG0, 0x00, CC1101_FIFOTHR, 0x07, CC1101_SYNC1, 0x54,
    CC1101_SYNC0, 0x3D, CC1101_PKTLEN, 0xFF, CC1101_PKTCTRL1, 0x00, CC1101_PKTCTRL0, 0x02,
    CC1101_ADDR, 0x00, CC1101_CHANNR, 0x00, CC1101_FSCTRL1, 0x08, CC1101_FSCTRL0, 0x00,
    CC1101_FREQ2, 0x21, CC1101_FREQ1, 0x65, CC1101_FREQ0, 0x6A, CC1101_MDMCFG4, 0x5C,
    CC1101_MDMCFG3, 0x04, CC1101_MDMCFG2, 0x05, CC1101_MDMCFG1, 0x22, CC1101_MDMCFG0, 0xF8,
    CC1101_DEVIATN, 0x44, CC1101_MCSM2, 0x07, CC1101_MCSM1, 0x00, CC1101_MCSM0, 0x18,
    CC1101_FOCCFG, 0x2E, CC1101_BSCFG, 0xBF, CC1101_AGCCTRL2, 0x43, CC1101_AGCCTRL1, 0x09,
    CC1101_AGCCTRL0, 0xB5, CC1101_WOREVT1, 0x87, CC1101_WOREVT0, 0x6B, CC1101_WORCTRL, 0xFB,
    CC1101_FREND1, 0xB6, CC1101_FREND0, 0x10, CC1101_FSCAL3, 0xEA, CC1101_FSCAL2, 0x2A,
    CC1101_FSCAL1, 0x00, CC1101_FSCAL0, 0x1F, CC1101_RCCTRL1, 0x41, CC1101_RCCTRL0, 0x00,
    CC1101_FSTEST, 0x59, CC1101_PTEST, 0x7F, CC1101_AGCTEST, 0x3F, CC1101_TEST2, 0x81,
    CC1101_TEST1, 0x35, CC1101_TEST0, 0x09, 0U, 0U, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00,
};
