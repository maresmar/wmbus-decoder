#include "wmbus_radio_rx_service.h"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_spi.h>
#include <string.h>

#include <lib/drivers/cc1101_regs.h>

#define TAG "WmBusDecoder"

#define WMBUS_FREQ_HZ                      868950000UL
#define WMBUS_FIFO_CHUNK                   64U
#define WMBUS_RXBYTES_MAX                  (WMBUS_FIFO_CHUNK + 1U)
#define WMBUS_STATUS_STABLE_READ_MAX       8U
#define WMBUS_RSSI_UPDATE_HZ               5U
#define WMBUS_LED_PULSE_MS                 40U
#define WMBUS_T_GAP_TIMEOUT_MS             5U
#define WMBUS_C_READ_TIMEOUT_MS            25U
#define WMBUS_CC1101_PKTCTRL0_INFINITE_LEN 0x02U
#define WMBUS_CC1101_PKTCTRL0_T_MODE       WMBUS_CC1101_PKTCTRL0_INFINITE_LEN
#define WMBUS_CC1101_PKTCTRL0_C_MODE       WMBUS_CC1101_PKTCTRL0_INFINITE_LEN
#define WMBUS_CC1101_MDMCFG2_2FSK_SYNC_16_CS 0x06U
#define WMBUS_CC1101_PKTSTATUS_SFD          0x08U
#define WMBUS_CC1101_MARCSTATE_MASK         0x1FU
#define WMBUS_CC1101_MARCSTATE_RX           0x0DU
#define WMBUS_CC1101_MARCSTATE_RX_END       0x0EU
#define WMBUS_CC1101_MARCSTATE_RX_RST       0x0FU

typedef enum {
    WmBusControlCmdStop = 0,
    WmBusControlCmdApplyConfig,
} WmBusControlCmd;

typedef struct {
    WmBusControlCmd cmd;
    WmBusSettings settings;
    WmBusCryptoKeyStore key_store;
} WmBusControlEvent;

typedef struct {
    const char* name;
    const uint8_t* regs;
    size_t regs_size;
} WmBusCc1101Profile;

struct WmBusRadioRxService {
    WmBusRadioRxCallbacks callbacks;
    FuriThread* thread;
    FuriMessageQueue* control_queue;
    WmBusSettings settings;
    WmBusCryptoKeyStore key_store;
};

static const uint8_t wmbus_cc1101_t_mode_preset_regs[] = {
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
    0x00,
    CC1101_PKTCTRL0,
    WMBUS_CC1101_PKTCTRL0_T_MODE,
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

static const uint8_t wmbus_cc1101_c_mode_preset_regs[] = {
    // Match the common first 16 bits of the C-mode sync (54 3D) exactly. The
    // capture layer validates the Frame-A (54 CD) or Frame-B (54 3D) remainder.
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
    0x00,
    CC1101_PKTCTRL0,
    WMBUS_CC1101_PKTCTRL0_C_MODE,
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
    WMBUS_CC1101_MDMCFG2_2FSK_SYNC_16_CS,
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

static const WmBusCc1101Profile wmbus_cc1101_t_mode_profile = {
    .name = "T",
    .regs = wmbus_cc1101_t_mode_preset_regs,
    .regs_size = sizeof(wmbus_cc1101_t_mode_preset_regs),
};

static const WmBusCc1101Profile wmbus_cc1101_c_mode_profile = {
    .name = "C",
    .regs = wmbus_cc1101_c_mode_preset_regs,
    .regs_size = sizeof(wmbus_cc1101_c_mode_preset_regs),
};

static const WmBusCc1101Profile* wmbus_cc1101_active_profile = &wmbus_cc1101_t_mode_profile;

static const WmBusCc1101Profile* wmbus_radio_profile(WmBusRxMode mode) {
    return mode == WmBusRxModeC ? &wmbus_cc1101_c_mode_profile : &wmbus_cc1101_t_mode_profile;
}

static void wmbus_radio_select_profile(WmBusRxMode mode) {
    wmbus_cc1101_active_profile = wmbus_radio_profile(mode);
}

static bool wmbus_radio_preset_loadable(const WmBusCc1101Profile* profile) {
    if(!profile || !profile->regs || profile->regs_size < 10U) return false;

    for(size_t i = 0; i + 1U < profile->regs_size; i += 2U) {
        uint8_t reg = profile->regs[i];
        uint8_t value = profile->regs[i + 1U];

        if(reg == 0U && value == 0U) {
            return i + 10U <= profile->regs_size;
        }

        if(reg == 0U) {
            return false;
        }
    }

    return false;
}

static bool
    wmbus_radio_profile_get_reg(const WmBusCc1101Profile* profile, uint8_t reg, uint8_t* value) {
    if(!profile || !profile->regs || !value) return false;

    for(size_t i = 0; i + 1U < profile->regs_size; i += 2U) {
        uint8_t profile_reg = profile->regs[i];
        uint8_t profile_value = profile->regs[i + 1U];

        if(profile_reg == 0U && profile_value == 0U) return false;
        if(profile_reg == reg) {
            *value = profile_value;
            return true;
        }
    }

    return false;
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
    furi_hal_subghz_load_custom_preset(wmbus_cc1101_active_profile->regs);
    furi_hal_subghz_set_frequency_and_path(WMBUS_FREQ_HZ);
    furi_hal_subghz_flush_rx();
    furi_hal_subghz_rx();
}

static char wmbus_radio_mode_char(WmBusRxMode mode) {
    return mode == WmBusRxModeC ? 'C' : 'T';
}

static bool wmbus_radio_validate_mode_regs(WmBusRxMode mode) {
    const WmBusCc1101Profile* profile = wmbus_radio_profile(mode);
    uint8_t expected_iocfg0 = 0U;
    uint8_t expected_pktctrl0 = 0U;
    uint8_t expected_pktctrl1 = 0U;
    uint8_t expected_mdmcfg2 = 0U;
    uint8_t expected_sync1 = 0U;
    uint8_t expected_sync0 = 0U;

    bool expected_ok = wmbus_radio_profile_get_reg(profile, CC1101_IOCFG0, &expected_iocfg0) &&
                       wmbus_radio_profile_get_reg(profile, CC1101_PKTCTRL0, &expected_pktctrl0) &&
                       wmbus_radio_profile_get_reg(profile, CC1101_PKTCTRL1, &expected_pktctrl1) &&
                       wmbus_radio_profile_get_reg(profile, CC1101_MDMCFG2, &expected_mdmcfg2) &&
                       wmbus_radio_profile_get_reg(profile, CC1101_SYNC1, &expected_sync1) &&
                       wmbus_radio_profile_get_reg(profile, CC1101_SYNC0, &expected_sync0);

    uint8_t iocfg0 = wmbus_radio_read_reg(CC1101_IOCFG0);
    uint8_t pktctrl0 = wmbus_radio_read_reg(CC1101_PKTCTRL0);
    uint8_t pktctrl1 = wmbus_radio_read_reg(CC1101_PKTCTRL1);
    uint8_t mdmcfg2 = wmbus_radio_read_reg(CC1101_MDMCFG2);
    uint8_t sync1 = wmbus_radio_read_reg(CC1101_SYNC1);
    uint8_t sync0 = wmbus_radio_read_reg(CC1101_SYNC0);

    FURI_LOG_D(
        TAG,
        "%c cfg SYNC=%02X%02X IOCFG0=%02X PKTCTRL0=%02X PKTCTRL1=%02X MDMCFG2=%02X",
        wmbus_radio_mode_char(mode),
        sync1,
        sync0,
        iocfg0,
        pktctrl0,
        pktctrl1,
        mdmcfg2);

    bool ok = expected_ok && (iocfg0 == expected_iocfg0) && (pktctrl0 == expected_pktctrl0) &&
              (pktctrl1 == expected_pktctrl1) && (mdmcfg2 == expected_mdmcfg2) &&
              (sync1 == expected_sync1) && (sync0 == expected_sync0);
    if(!ok) {
        FURI_LOG_W(
            TAG,
            "%c cfg mismatch (SYNC=%02X%02X/%02X%02X IOCFG0=%02X/%02X PKTCTRL0=%02X/%02X PKTCTRL1=%02X/%02X MDMCFG2=%02X/%02X)",
            wmbus_radio_mode_char(mode),
            sync1,
            sync0,
            expected_sync1,
            expected_sync0,
            iocfg0,
            expected_iocfg0,
            pktctrl0,
            expected_pktctrl0,
            pktctrl1,
            expected_pktctrl1,
            mdmcfg2,
            expected_mdmcfg2);
    }
    return ok;
}

static void wmbus_radio_apply_mode(WmBusRxMode mode) {
    wmbus_radio_select_profile(mode);
    furi_check(wmbus_radio_preset_loadable(wmbus_cc1101_active_profile));

    wmbus_radio_reload_rx_preset();
    if(!wmbus_radio_validate_mode_regs(mode)) {
        wmbus_radio_reload_rx_preset();
        if(!wmbus_radio_validate_mode_regs(mode)) {
            FURI_LOG_W(
                TAG, "%c cfg still mismatched after preset reload", wmbus_radio_mode_char(mode));
            wmbus_radio_recover_rx();
        }
    }
}

static uint8_t wmbus_radio_read_status_locked(uint8_t reg) {
    uint8_t cmd[2] = {reg | CC1101_READ | CC1101_BURST, 0U};
    furi_hal_spi_bus_trx(
        &furi_hal_spi_bus_handle_subghz, cmd, cmd, sizeof(cmd), CC1101_TIMEOUT);
    return cmd[1];
}

static bool wmbus_radio_read_status_stable(uint8_t reg, uint8_t* out_value) {
    if(!out_value) return false;

    bool stable = false;
    uint8_t value = 0U;
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz);
    uint8_t previous = wmbus_radio_read_status_locked(reg);
    for(size_t i = 0U; i < WMBUS_STATUS_STABLE_READ_MAX; i++) {
        value = wmbus_radio_read_status_locked(reg);
        if(value == previous) {
            stable = true;
            break;
        }
        previous = value;
    }
    furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz);

    if(stable) *out_value = value;
    return stable;
}

static bool wmbus_radio_read_rxbytes_stable(uint8_t* out_count, bool* out_overflow) {
    if(!out_count || !out_overflow) return false;

    /* CC1101 errata SWRZ020: RXBYTES is continuously updated across clock
     * domains and is trusted only after the same value is observed twice. */
    uint8_t value = 0U;
    if(!wmbus_radio_read_status_stable(CC1101_STATUS_RXBYTES, &value)) return false;
    *out_overflow = (value & 0x80U) != 0U;
    *out_count = value & 0x7FU;
    /* RXBYTES includes the one-byte prefetch buffer, so 65 is valid even
     * though an SPI FIFO read is limited to the 64-byte FIFO capacity. */
    return *out_overflow || *out_count <= WMBUS_RXBYTES_MAX;
}

static bool wmbus_radio_marcstate_is_rx(uint8_t raw_state) {
    switch(raw_state & WMBUS_CC1101_MARCSTATE_MASK) {
    case WMBUS_CC1101_MARCSTATE_RX:
    case WMBUS_CC1101_MARCSTATE_RX_END:
    case WMBUS_CC1101_MARCSTATE_RX_RST:
        return true;
    default:
        return false;
    }
}

static bool wmbus_radio_read_fifo_exact(uint8_t* data, uint8_t size) {
    if(!data || size == 0U || size > WMBUS_FIFO_CHUNK) return false;

    uint8_t addr = CC1101_FIFO | CC1101_READ | CC1101_BURST;
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz);
    furi_hal_spi_bus_tx(&furi_hal_spi_bus_handle_subghz, &addr, 1U, CC1101_TIMEOUT);
    furi_hal_spi_bus_rx(&furi_hal_spi_bus_handle_subghz, data, size, CC1101_TIMEOUT);
    furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz);
    return true;
}

static bool wmbus_capture_read_fifo(
    uint8_t* raw,
    size_t* raw_len,
    size_t raw_max,
    size_t read_size,
    bool* had_data,
    uint32_t* last_activity_tick) {
    uint8_t temp[WMBUS_FIFO_CHUNK];
    if(!raw || !raw_len || !had_data || !last_activity_tick || *raw_len > raw_max ||
       read_size == 0U || read_size > sizeof(temp) || read_size > raw_max - *raw_len) {
        return false;
    }
    if(!wmbus_radio_read_fifo_exact(temp, (uint8_t)read_size)) return false;

    memcpy(&raw[*raw_len], temp, read_size);
    *raw_len += read_size;

    *had_data = true;
    *last_activity_tick = furi_get_tick();
    return true;
}

static bool wmbus_capture_step(
    WmBusFifoCaptureState* state,
    WmBusPhyFrame* frame,
    bool* had_data,
    uint32_t gap_ticks,
    WmBusRxMode mode) {
    if(!state || !frame || !had_data) return false;

    bool invalid = false;
    bool complete = false;
    size_t expected_fifo_len = 0U;
    WmBusFrameFormat format = WmBusFrameFormatUnknown;

    while(true) {
        WmBusCaptureLengthStatus length_status = wmbus_fifo_frame_length(
            mode, state->raw, state->raw_len, &expected_fifo_len, &format);
        if(length_status == WmBusCaptureLengthInvalid) {
            invalid = true;
            break;
        }
        if(length_status == WmBusCaptureLengthKnown && state->raw_len >= expected_fifo_len) {
            complete = true;
            break;
        }

        uint8_t available = 0U;
        bool overflow = false;
        if(!wmbus_radio_read_rxbytes_stable(&available, &overflow)) break;
        if(overflow) {
            wmbus_fifo_capture_state_reset(state);
            wmbus_radio_recover_rx();
            return false;
        }

        if(available > 0U) {
            if(!state->in_packet) {
                wmbus_fifo_capture_note_activity(state, furi_get_tick());
            }
        } else if(!state->in_packet) {
            /* In infinite-length mode SFD can remain asserted after a false
             * sync even when no full byte reaches RX FIFO. Start the same
             * bounded incomplete-frame timeout from the hardware sync. */
            uint8_t pktstatus = 0U;
            if(wmbus_radio_read_status_stable(CC1101_STATUS_PKTSTATUS, &pktstatus) &&
               (pktstatus & WMBUS_CC1101_PKTSTATUS_SFD) != 0U) {
                wmbus_fifo_capture_note_activity(state, furi_get_tick());
            }
        }

        size_t read_size = wmbus_fifo_safe_read_size(
            mode, length_status, state->raw_len, expected_fifo_len, available);
        if(read_size == 0U) break;

        if(!wmbus_capture_read_fifo(
               state->raw,
               &state->raw_len,
               sizeof(state->raw),
               read_size,
               had_data,
               &state->last_activity_tick)) {
            break;
        }

        state->in_packet = true;
    }

    if(invalid) {
        wmbus_fifo_capture_state_reset(state);
        wmbus_radio_recover_rx();
        return false;
    }
    if(!state->in_packet) return false;

    uint32_t now = furi_get_tick();
    bool timed_out = (!*had_data && (now - state->last_activity_tick) >= gap_ticks);
    if(!complete) {
        if(!timed_out) return false;
        FURI_LOG_D(
            TAG,
            "%c incomplete RX timeout: captured=%u",
            wmbus_radio_mode_char(mode),
            (unsigned int)state->raw_len);
        wmbus_fifo_capture_state_reset(state);
        wmbus_radio_recover_rx();
        return false;
    }

    bool frame_valid = wmbus_phy_frame_from_fifo(
        mode, state->raw, state->raw_len, (int)furi_hal_subghz_get_rssi(), frame);
    wmbus_fifo_capture_state_reset(state);
    wmbus_radio_recover_rx();
    return frame_valid;
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

    WmBusFifoCaptureState capture_t = {0};
    WmBusFifoCaptureState capture_c = {0};
    wmbus_fifo_capture_state_reset(&capture_t);
    wmbus_fifo_capture_state_reset(&capture_c);

    uint32_t t_gap_ticks = wmbus_ticks_from_ms(WMBUS_T_GAP_TIMEOUT_MS);
    uint32_t c_gap_ticks = wmbus_ticks_from_ms(WMBUS_C_READ_TIMEOUT_MS);
    uint32_t rssi_ticks = wmbus_ticks_from_ms(1000U / WMBUS_RSSI_UPDATE_HZ);
    uint32_t led_pulse_ticks = wmbus_ticks_from_ms(WMBUS_LED_PULSE_MS);

    uint32_t last_rssi_tick = 0U;
    uint8_t rssi_log_divider = 0U;
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
                    wmbus_fifo_capture_state_reset(&capture_t);
                    wmbus_fifo_capture_state_reset(&capture_c);
                }
            }
        }

        if(!running) break;

        bool had_data = false;
        WmBusPhyFrame frame = {0};
        bool frame_ready =
            (mode == WmBusRxModeT) ?
                wmbus_capture_step(&capture_t, &frame, &had_data, t_gap_ticks, WmBusRxModeT) :
                wmbus_capture_step(&capture_c, &frame, &had_data, c_gap_ticks, WmBusRxModeC);

        if(frame_ready) {
            uint32_t pulse_now = furi_get_tick();
            furi_hal_light_set(LightGreen, 0xFF);
            led_pulse_on = true;
            led_pulse_off_tick = pulse_now + led_pulse_ticks;
            if(service->callbacks.handle_frame) {
                service->callbacks.handle_frame(
                    service->callbacks.context, &runtime_settings, &runtime_key_store, &frame);
            }
        }

        uint32_t now_tick = furi_get_tick();
        if(led_pulse_on && ((int32_t)(now_tick - led_pulse_off_tick) >= 0)) {
            furi_hal_light_set(LightGreen, 0x00);
            led_pulse_on = false;
        }

        if(last_rssi_tick == 0U || (now_tick - last_rssi_tick) >= rssi_ticks) {
            int live_rssi = (int)furi_hal_subghz_get_rssi();
            uint8_t marcstate = 0U;
            bool marcstate_valid =
                wmbus_radio_read_status_stable(CC1101_STATUS_MARCSTATE, &marcstate);
            if(marcstate_valid && !wmbus_radio_marcstate_is_rx(marcstate)) {
                FURI_LOG_W(
                    TAG,
                    "%c RX watchdog recovery: MARCSTATE=%02X capture=%u",
                    wmbus_radio_mode_char(mode),
                    (unsigned int)(marcstate & WMBUS_CC1101_MARCSTATE_MASK),
                    (unsigned int)(mode == WmBusRxModeT ? capture_t.raw_len :
                                                              capture_c.raw_len));
                wmbus_fifo_capture_state_reset(
                    mode == WmBusRxModeT ? &capture_t : &capture_c);
                wmbus_radio_recover_rx();
            }

            if(service->callbacks.set_live_rssi) {
                service->callbacks.set_live_rssi(service->callbacks.context, live_rssi);
            }

            if(++rssi_log_divider >= WMBUS_RSSI_UPDATE_HZ) {
                uint8_t pktstatus = 0U;
                uint8_t rxbytes = 0U;
                bool overflow = false;
                bool pktstatus_valid =
                    wmbus_radio_read_status_stable(CC1101_STATUS_PKTSTATUS, &pktstatus);
                bool rxbytes_valid = wmbus_radio_read_rxbytes_stable(&rxbytes, &overflow);
                FURI_LOG_D(
                    TAG,
                    "%c RX health: RSSI=%d MARC=%02X PKT=%02X RXB=%u%s capture=%u",
                    wmbus_radio_mode_char(mode),
                    live_rssi,
                    (unsigned int)(marcstate_valid ?
                                       (marcstate & WMBUS_CC1101_MARCSTATE_MASK) :
                                       0xFFU),
                    (unsigned int)(pktstatus_valid ? pktstatus : 0xFFU),
                    (unsigned int)(rxbytes_valid ? rxbytes : 0xFFU),
                    overflow ? "!" : "",
                    (unsigned int)(mode == WmBusRxModeT ? capture_t.raw_len :
                                                              capture_c.raw_len));
                rssi_log_divider = 0U;
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
    furi_thread_set_priority(service->thread, FuriThreadPriorityHigh);

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
    return furi_message_queue_put(service->control_queue, &event, FuriWaitForever) == FuriStatusOk;
}
