#include "wmbus_view.h"

#include <furi.h>
#include <gui/canvas.h>
#include <input/input.h>
#include <stdio.h>

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

static void wmbus_view_draw(Canvas* canvas, void* model) {
    WmBusViewModel* m = model;
    char line[64];
    const char* sync_label = wmbus_sync_label(m->sync_index);
    uint8_t display_cursor = m->hist_cursor;
    if(m->hist_count > 0 && display_cursor >= m->hist_count) {
        display_cursor = (uint8_t)(m->hist_count - 1U);
    }
    const WmBusHistoryEntry* entry = wmbus_history_get(m, display_cursor);
    const char* mode_label = entry ? (entry->used_3of6 ? "T" : "C") : sync_label;
    uint8_t hist_pos = entry ? (uint8_t)(display_cursor + 1U) : 0;
    uint8_t hist_total = WMBUS_HIST_MAX;
    const char* live_label = m->freeze_display ? "PAUSE" : "LIVE";

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 8, "WM-Bus RX");
    int32_t live_width = (int32_t)canvas_string_width(canvas, live_label);
    int32_t right = (int32_t)canvas_width(canvas);
    int32_t live_x = right - live_width;
    if(live_x < 0) live_x = 0;
    canvas_draw_str(canvas, live_x, 8, live_label);

    canvas_set_font(canvas, FontSecondary);

    if(!m->freq_valid) {
        canvas_draw_str(canvas, 0, 22, "Freq not allowed");
        canvas_draw_str(canvas, 0, 32, "Check region");
        return;
    }

    snprintf(
        line,
        sizeof(line),
        "Dec:%lu OK:%lu BAD:%lu",
        m->packets_decoded,
        m->packets_crc_ok,
        m->packets_crc_bad);
    canvas_draw_str(canvas, 0, 18, line);

    char rate[8];
    if(m->packets_per_sec > 0) {
        snprintf(rate, sizeof(rate), "%u/s", (unsigned int)m->packets_per_sec);
    } else {
        snprintf(rate, sizeof(rate), "--/s");
    }

    snprintf(line, sizeof(line), "Str:%lu R:%s H:%u/%u", m->packets_strong, rate, hist_pos, hist_total);
    canvas_draw_str(canvas, 0, 28, line);

    snprintf(
        line,
        sizeof(line),
        "Status:%s M:%s C:%u",
        wmbus_status_str(m->last_status),
        mode_label,
        m->last_confidence);
    canvas_draw_str(canvas, 0, 38, line);

    if(!entry) {
        canvas_draw_str(canvas, 0, 48, "Waiting for RX...");
        snprintf(line, sizeof(line), "868.95 MHz 100kbps %s", sync_label);
        canvas_draw_str(canvas, 0, 58, line);
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
        snprintf(line, sizeof(line), "MFG:%s ID:%s", entry->mfg, entry->id_str);
        canvas_draw_str(canvas, 0, 48, line);
        snprintf(
            line,
            sizeof(line),
            "L:%02X C:%02X CI:%02X R:%d",
            entry->l_field,
            entry->c_field,
            entry->ci_field,
            entry->rssi);
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
            uint8_t idx = (uint8_t)(
                (m->rssi_hist_head + WMBUS_RSSI_HISTORY - (count - 1U - i)) %
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
        *ctx->mode_request = 0;
        *ctx->mode_change = true;
        return true;
    }

    if(event->type == InputTypeShort && event->key == InputKeyRight) {
        *ctx->mode_request = 1;
        *ctx->mode_change = true;
        return true;
    }

    if(event->type == InputTypeShort && event->key == InputKeyUp) {
        with_view_model(
            ctx->view,
            WmBusViewModel * model,
            {
                if(model->hist_cursor + 1U < model->hist_count) {
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
                if(model->hist_cursor > 0) {
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
            ctx->view,
            WmBusViewModel * model,
            { model->debug_mode = !model->debug_mode; },
            true);
        return true;
    }

    return false;
}

void wmbus_view_setup(View* view, WmBusViewContext* ctx) {
    view_set_context(view, ctx);
    view_set_draw_callback(view, wmbus_view_draw);
    view_set_input_callback(view, wmbus_view_input);
}
