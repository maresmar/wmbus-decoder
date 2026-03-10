#include "wmbus_view.h"

#include <furi.h>
#include <gui/canvas.h>
#include <input/input.h>
#include <stdio.h>

static bool wmbus_send_control(WmBusViewContext* ctx, WmBusControlCmd cmd) {
    if(!ctx->control_queue) return false;
    return furi_message_queue_put(ctx->control_queue, &cmd, 0) == FuriStatusOk;
}

static const char* wmbus_status_str(WmBusStatus status) {
    switch(status) {
    case WmBusStatusDecodeFail:
        return "Decode fail";
    case WmBusStatusNotPlausible:
        return "Not plausible";
    case WmBusStatusFramingError:
        return "Framing error";
    case WmBusStatusCrcBad:
        return "CRC BAD";
    case WmBusStatusWeakRssi:
        return "Weak RSSI";
    case WmBusStatusOk:
        return "OK";
    default:
        return "--";
    }
}

static const char* wmbus_sync_label(uint8_t sync_index) {
    switch(sync_index) {
    case 0:
        return "T";
    case 1:
        return "C";
    default:
        return "??";
    }
}

static const WmBusHistoryEntry* wmbus_history_get(const WmBusViewModel* model, uint8_t cursor) {
    if(model->hist_count == 0) return NULL;
    if(cursor >= model->hist_count) cursor = (uint8_t)(model->hist_count - 1U);
    uint8_t index = (uint8_t)((model->hist_head + WMBUS_HIST_MAX - cursor) % WMBUS_HIST_MAX);
    return &model->hist[index];
}

static char
    wmbus_security_flag(bool has_short_tpl, uint8_t security_mode, bool security_likely_encrypted) {
    if(!has_short_tpl) return '-';
    if(security_likely_encrypted) return 'Y';
    return security_mode ? '?' : 'N';
}

static void wmbus_format_entry_age(const WmBusHistoryEntry* entry, char* out, size_t out_size) {
    if(out_size == 0U) return;
    out[0] = '\0';

    if(!entry || entry->rx_tick == 0U) {
        return;
    }

    uint32_t tick_freq = furi_kernel_get_tick_frequency();
    if(tick_freq == 0U) {
        return;
    }

    uint32_t age_seconds = (furi_get_tick() - entry->rx_tick) / tick_freq;
    if(age_seconds < 60U) {
        snprintf(out, out_size, "%lus", (unsigned long)age_seconds);
    } else if(age_seconds < 3600U) {
        snprintf(out, out_size, "%lum", (unsigned long)(age_seconds / 60U));
    } else if(age_seconds < 86400U) {
        snprintf(out, out_size, "%luh", (unsigned long)(age_seconds / 3600U));
    } else {
        snprintf(out, out_size, "%lud", (unsigned long)(age_seconds / 86400U));
    }
}

static void wmbus_view_draw(Canvas* canvas, void* model) {
    WmBusViewModel* m = model;
    char line[64];
    char age[12];
    char mode_header[16];
    const char* sync_label = wmbus_sync_label(m->sync_index);
    uint8_t display_cursor = m->hist_cursor;
    if(display_cursor >= m->hist_count && !m->freeze_display) {
        display_cursor = (uint8_t)(m->hist_count - 1U);
    }
    const WmBusHistoryEntry* entry = wmbus_history_get(m, display_cursor);
    if(m->freeze_display) {
        if(entry) {
            uint8_t hist_pos = (uint8_t)(display_cursor + 1U);
            snprintf(mode_header, sizeof(mode_header), "H:%u/%u", hist_pos, m->hist_count);
        } else {
            snprintf(mode_header, sizeof(mode_header), "H:-/%u", m->hist_count);
        }
    } else {
        snprintf(mode_header, sizeof(mode_header), "Latest");
    }

    wmbus_format_entry_age(entry, age, sizeof(age));

    // Title
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 8, "WM-Bus RX");
    canvas_draw_str_aligned(canvas, canvas_width(canvas), 8, AlignRight, AlignBottom, mode_header);

    canvas_set_font(canvas, FontSecondary);

    if(!m->freq_valid) {
        canvas_draw_str(canvas, 0, 22, "Freq not allowed");
        canvas_draw_str(canvas, 0, 32, "Check region");
        return;
    }

    snprintf(
        line,
        sizeof(line),
        "DEC:%lu OK:%lu BAD:%lu",
        m->packets_decoded,
        m->packets_crc_ok,
        m->packets_crc_bad);
    canvas_draw_str(canvas, 0, 18, line);
    snprintf(line, sizeof(line), "Rhi:%lu", m->packets_strong);
    canvas_draw_str_aligned(canvas, canvas_width(canvas), 18, AlignRight, AlignBottom, line);

    snprintf(
        line,
        sizeof(line),
        "Lst M:%s R:%u/s RSSI:%d",
        sync_label,
        (unsigned int)m->packets_per_sec,
        m->rssi);
    canvas_draw_str(canvas, 0, 28, line);

    //canvas_draw_line(canvas, 0, 29, canvas_width(canvas), 29);

    WmBusStatus displayed_status = (m->freeze_display && entry) ? entry->status : m->last_status;
    snprintf(
        line,
        sizeof(line),
        "%s %s",
        m->freeze_display ? "Pkt" : "Last",
        wmbus_status_str(displayed_status));
    canvas_draw_str(canvas, 0, 38, line);
    if(age[0] != '\0') {
        snprintf(line, sizeof(line), "A:%s", age);
        canvas_draw_str_aligned(canvas, canvas_width(canvas), 38, AlignRight, AlignBottom, line);
    }

    if(!entry) {
        canvas_draw_str(canvas, 0, 48, "Waiting for RX...");
        canvas_draw_str(canvas, 0, 58, "868.95 MHz 100kbps");
    } else if(m->debug_mode) {
        snprintf(
            line,
            sizeof(line),
            "L:%02X C:%02X CI:%02X V:%02X",
            entry->l_field,
            entry->c_field,
            entry->ci_field,
            entry->version);
        canvas_draw_str(canvas, 0, 48, line);

        char hex[WMBUS_FRAME_PREVIEW_MAX * 2 + 1];
        size_t show_len = entry->frame_preview_len;
        if(show_len > 8) show_len = 8;
        for(size_t i = 0; i < show_len; i++) {
            snprintf(&hex[i * 2], 3, "%02X", entry->frame_preview[i]);
        }
        hex[show_len * 2] = '\0';
        const int hex_chars = (int)(show_len * 2U);
        snprintf(
            line,
            sizeof(line),
            "Hex:%.*s%s",
            hex_chars,
            hex,
            (entry->frame_preview_len > show_len) ? "..." : "");
        canvas_draw_str(canvas, 0, 58, line);
    } else {
        snprintf(
            line, sizeof(line), "MF:%s DT:%02X ID:%s", entry->mfg, entry->dev_type, entry->id_str);
        canvas_draw_str(canvas, 0, 48, line);
        if(entry->has_total_m3) {
            uint32_t whole = entry->total_m3_x1000 / 1000U;
            uint32_t frac = entry->total_m3_x1000 % 1000U;
            snprintf(line, sizeof(line), "Tot:%lu.%03lum3 R:%d", whole, frac, entry->rssi);
        } else if(entry->has_short_tpl) {
            snprintf(
                line,
                sizeof(line),
                "CI:%02X S:%02X E:%c R:%d",
                entry->ci_field,
                entry->security_mode,
                wmbus_security_flag(
                    entry->has_short_tpl, entry->security_mode, entry->security_likely_encrypted),
                entry->rssi);
        } else {
            snprintf(
                line,
                sizeof(line),
                "L:%02X C:%02X CI:%02X M:%s R:%d",
                entry->l_field,
                entry->c_field,
                entry->ci_field,
                (entry->used_3of6 ? "T" : "C"),
                entry->rssi);
        }
        canvas_draw_str(canvas, 0, 58, line);
    }

    if(m->rssi_hist_count > 0) {
        const int rssi_min = -110;
        const int rssi_max = -40;
        const uint8_t graph_base = 63;
        const uint8_t graph_height = 6;
        uint8_t count = m->rssi_hist_count;
        if(count > WMBUS_RSSI_HISTORY) count = WMBUS_RSSI_HISTORY;
        for(uint8_t i = 0; i < count; i++) {
            uint8_t idx = (uint8_t)((m->rssi_hist_head + WMBUS_RSSI_HISTORY - (count - 1U - i)) %
                                    WMBUS_RSSI_HISTORY);
            int rssi = m->rssi_hist[idx];
            if(rssi < rssi_min) rssi = rssi_min;
            if(rssi > rssi_max) rssi = rssi_max;
            uint8_t height = (uint8_t)(((rssi - rssi_min) * graph_height) / (rssi_max - rssi_min));
            uint8_t x = (uint8_t)(i * 2U);
            canvas_draw_line(canvas, x, graph_base, x, (uint8_t)(graph_base - height));
        }
    }
}

static bool wmbus_view_input(InputEvent* event, void* context) {
    furi_assert(context);
    WmBusViewContext* ctx = context;

    if(event->type == InputTypeShort && event->key == InputKeyBack) {
        view_dispatcher_stop(ctx->view_dispatcher);
        return true;
    }

    if(event->type == InputTypeShort && event->key == InputKeyLeft) {
        wmbus_send_control(ctx, WmBusControlCmdSetModeT);
        return true;
    }

    if(event->type == InputTypeShort && event->key == InputKeyRight) {
        wmbus_send_control(ctx, WmBusControlCmdSetModeC);
        return true;
    }

    if(event->type == InputTypeShort && event->key == InputKeyUp) {
        with_view_model(
            ctx->view,
            WmBusViewModel * model,
            {
                if(model->freeze_display && model->hist_cursor + 1U < model->hist_count) {
                    model->hist_cursor++;
                }
            },
            true);
        return true;
    }

    if(event->type == InputTypeShort && event->key == InputKeyDown) {
        with_view_model(
            ctx->view,
            WmBusViewModel * model,
            {
                if(model->freeze_display && model->hist_cursor > 0) {
                    model->hist_cursor--;
                }
            },
            true);
        return true;
    }

    if(event->type == InputTypeShort && event->key == InputKeyOk) {
        with_view_model(
            ctx->view,
            WmBusViewModel * model,
            {
                model->freeze_display = !model->freeze_display;
                if(!model->freeze_display) {
                    model->hist_cursor = 0;
                }
            },
            true);
        return true;
    }

    if(event->type == InputTypeLong && event->key == InputKeyUp) {
        with_view_model(
            ctx->view, WmBusViewModel * model, { model->debug_mode = !model->debug_mode; }, true);
        return true;
    }

    return false;
}

void wmbus_view_setup(View* view, WmBusViewContext* ctx) {
    view_set_context(view, ctx);
    view_set_draw_callback(view, wmbus_view_draw);
    view_set_input_callback(view, wmbus_view_input);
}
