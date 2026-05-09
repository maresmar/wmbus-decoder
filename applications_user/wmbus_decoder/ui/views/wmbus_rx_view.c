#include "wmbus_rx_view.h"

#include "../../protocol/format/wmbus_packet_formatter.h"
#include "../../protocol/format/wmbus_packet_summary.h"
#include "../../protocol/model/wmbus_application_record.h"
#include "../../protocol/parser/wmbus_parser.h"

#include <furi.h>
#include <gui/canvas.h>
#include <input/input.h>
#include <stdio.h>
#include <string.h>

#define WMBUS_HIST_MAX            40U
#define WMBUS_RSSI_HISTORY        64U
#define WMBUS_RX_DEBUG_PACKET_MAX 16U

typedef struct {
    ViewDispatcher* view_dispatcher;
    View* view;
} WmBusRxViewContext;

typedef struct {
    bool packet_is_frame;
    WmBusStatus status;
    WmBusRxMode mode;
    int8_t rssi;
    uint32_t rx_tick;
    uint16_t packet_len;
    uint8_t packet_preview_len;
    uint8_t packet_preview[WMBUS_RX_DEBUG_PACKET_MAX];
    WmBusPacketDllData dll;
    WmBusPacketIdentityData identity;
    WmBusPacketEllData ell;
    WmBusPacketTplData tpl;
    WmBusPacketApplicationData application;
} WmBusRxHistoryEntry;

typedef struct {
    bool freq_valid;
    bool debug_mode;
    WmBusRxMode mode;
    int rssi;
    WmBusStatus last_status;
    uint32_t packets_decoded;
    uint32_t packets_strong;
    uint32_t packets_crc_ok;
    uint32_t packets_crc_bad;
    bool last_crc_valid;
    bool last_crc_ok;

    WmBusRxHistoryEntry latest;
    bool has_latest;

    WmBusRxHistoryEntry hist[WMBUS_HIST_MAX];
    uint8_t hist_count;
    uint8_t hist_head;
    uint8_t hist_cursor;
    bool freeze_display;

    int8_t rssi_hist[WMBUS_RSSI_HISTORY];
    uint8_t rssi_hist_count;
    uint8_t rssi_hist_head;
} WmBusRxViewModel;

struct WmBusRxView {
    View* view;
    WmBusRxViewContext context;
};

static bool wmbus_rx_send_event(WmBusRxViewContext* context, WmBusRxViewEvent event) {
    if(!context || !context->view_dispatcher) {
        return false;
    }

    view_dispatcher_send_custom_event(context->view_dispatcher, event);
    return true;
}

static const WmBusRxHistoryEntry*
    wmbus_rx_history_get(const WmBusRxViewModel* model, uint8_t cursor) {
    if(!model || model->hist_count == 0U) return NULL;
    if(cursor >= model->hist_count) cursor = (uint8_t)(model->hist_count - 1U);
    uint8_t index = (uint8_t)((model->hist_head + WMBUS_HIST_MAX - cursor) % WMBUS_HIST_MAX);
    return &model->hist[index];
}

static uint16_t wmbus_rx_history_preview_len(const WmBusRxHistoryEntry* entry) {
    if(!entry) return 0U;
    return entry->packet_preview_len;
}

static const WmBusRxHistoryEntry* wmbus_rx_display_entry_get(const WmBusRxViewModel* model) {
    if(!model) {
        return NULL;
    }

    if(model->freeze_display) {
        return wmbus_rx_history_get(model, model->hist_cursor);
    }

    if(model->has_latest) {
        return &model->latest;
    }

    return wmbus_rx_history_get(model, 0U);
}

static void wmbus_rx_format_age(const WmBusRxHistoryEntry* entry, char* out, size_t out_size) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';
    if(!entry || entry->rx_tick == 0U) return;

    uint32_t tick_freq = furi_kernel_get_tick_frequency();
    if(tick_freq == 0U) return;

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

static void
    wmbus_rx_preview_hex(const uint8_t* data, size_t data_len, char* out, size_t out_size) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';
    if(!data) return;

    size_t preview_len = data_len;
    if(preview_len > 8U) preview_len = 8U;

    size_t write = 0;
    for(size_t i = 0; i < preview_len && (write + 2U) < out_size; i++) {
        snprintf(&out[write], out_size - write, "%02X", data[i]);
        write += 2U;
    }
    out[write] = '\0';
}

static void
    wmbus_rx_format_footer_left(const WmBusRxHistoryEntry* entry, char* out, size_t out_size) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';
    if(!entry) return;

    char crypto[12] = {0};
    wmbus_packet_summary_format_crypto_tag(&entry->ell, &entry->tpl, crypto, sizeof(crypto));
    if(crypto[0] != '\0') {
        snprintf(out, out_size, "%s REC:%u", crypto, (unsigned int)entry->application.record_count);
    } else {
        snprintf(out, out_size, "REC:%u", (unsigned int)entry->application.record_count);
    }
}

static void
    wmbus_rx_format_footer_right(const WmBusRxHistoryEntry* entry, char* out, size_t out_size) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';
    if(!entry) return;

    uint32_t total_m3_x1000 = 0U;
    if(!wmbus_packet_summary_find_total_m3(&entry->application, &total_m3_x1000)) {
        return;
    }

    char volume[WMBUS_PACKET_VALUE_MAX] = {0};
    wmbus_packet_summary_format_total_m3(total_m3_x1000, volume, sizeof(volume), false);
    snprintf(out, out_size, "%sm3", volume);
}

static void wmbus_rx_draw(Canvas* canvas, void* model) {
    const WmBusRxViewModel* m = model;
    char line[64];
    char age[12];
    char header[16];

    const WmBusRxHistoryEntry* entry = wmbus_rx_display_entry_get(m);

    if(m->freeze_display) {
        uint8_t display_cursor = m->hist_cursor;
        if(display_cursor >= m->hist_count && m->hist_count > 0U) {
            display_cursor = (uint8_t)(m->hist_count - 1U);
        }
        if(entry) {
            snprintf(header, sizeof(header), "H:%u/%u", display_cursor + 1U, m->hist_count);
        } else {
            snprintf(header, sizeof(header), "H:-/0");
        }
    } else {
        snprintf(header, sizeof(header), "Latest");
    }

    wmbus_rx_format_age(entry, age, sizeof(age));

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 8, "WM-Bus RX");
    canvas_draw_str_aligned(canvas, canvas_width(canvas), 8, AlignRight, AlignBottom, header);

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
        "Status:%s",
        wmbus_packet_status_str(entry ? entry->status : m->last_status));
    canvas_draw_str(canvas, 0, 38, line);

    snprintf(line, sizeof(line), "RSSI:%d", entry ? entry->rssi : m->rssi);
    canvas_draw_str_aligned(canvas, canvas_width(canvas), 38, AlignRight, AlignBottom, line);

    if(age[0] != '\0') {
        snprintf(line, sizeof(line), "A:%s", age);
        canvas_draw_str_aligned(canvas, canvas_width(canvas), 28, AlignRight, AlignBottom, line);
    }

    if(!entry) {
        canvas_draw_str(canvas, 0, 48, "Waiting for RX...");
        canvas_draw_str(canvas, 0, 58, "OK=history  long OK=config");
    } else if(m->debug_mode) {
        char hex[WMBUS_PACKET_VALUE_MAX];
        wmbus_rx_preview_hex(
            entry->packet_preview, wmbus_rx_history_preview_len(entry), hex, sizeof(hex));
        snprintf(
            line,
            sizeof(line),
            "L:%02X C:%02X CI:%02X V:%02X",
            entry->dll.l_field,
            entry->dll.c_field,
            entry->dll.ci_field,
            entry->dll.version);
        canvas_draw_str(canvas, 0, 48, line);
        snprintf(line, sizeof(line), "Hex:%s%s", hex, (entry->packet_len > 8U) ? "..." : "");
        canvas_draw_str(canvas, 0, 58, line);
    } else if(!entry->packet_is_frame) {
        char footer_left[24];
        canvas_draw_str(canvas, 0, 48, "Raw packet");
        wmbus_rx_format_footer_left(entry, footer_left, sizeof(footer_left));
        canvas_draw_str(canvas, 0, 58, footer_left);
    } else {
        char right[20];
        char footer_left[24];
        char footer_right[24];
        snprintf(
            line,
            sizeof(line),
            "MFC:%s DT:%02X",
            entry->identity.manufacturer,
            entry->dll.dev_type);
        canvas_draw_str(canvas, 0, 48, line);
        snprintf(right, sizeof(right), "ID:%s", entry->identity.meter_id);
        canvas_draw_str_aligned(canvas, canvas_width(canvas), 48, AlignRight, AlignBottom, right);
        wmbus_rx_format_footer_left(entry, footer_left, sizeof(footer_left));
        wmbus_rx_format_footer_right(entry, footer_right, sizeof(footer_right));
        canvas_draw_str(canvas, 0, 58, footer_left);
        if(footer_right[0] != '\0') {
            canvas_draw_str_aligned(
                canvas, canvas_width(canvas), 58, AlignRight, AlignBottom, footer_right);
        }
    }

    if(m->rssi_hist_count > 0U) {
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

static bool wmbus_rx_input(InputEvent* event, void* context) {
    if(!event || !context) return false;
    WmBusRxViewContext* ctx = context;

    if(event->type == InputTypeShort && event->key == InputKeyUp && ctx->view) {
        with_view_model(
            ctx->view,
            WmBusRxViewModel * model,
            {
                if(model->freeze_display && model->hist_cursor + 1U < model->hist_count) {
                    model->hist_cursor++;
                }
            },
            true);
        return true;
    }

    if(event->type == InputTypeShort && event->key == InputKeyDown && ctx->view) {
        with_view_model(
            ctx->view,
            WmBusRxViewModel * model,
            {
                if(model->freeze_display && model->hist_cursor > 0U) {
                    model->hist_cursor--;
                }
            },
            true);
        return true;
    }

    if(event->type == InputTypeShort && event->key == InputKeyOk && ctx->view) {
        with_view_model(
            ctx->view,
            WmBusRxViewModel * model,
            {
                model->freeze_display = !model->freeze_display;
                if(!model->freeze_display) {
                    model->hist_cursor = 0U;
                }
            },
            true);
        return true;
    }

    if(event->type == InputTypeLong && event->key == InputKeyOk) {
        return wmbus_rx_send_event(ctx, WmBusRxViewEventOpenConfig);
    }

    if(event->type == InputTypeLong && event->key == InputKeyUp) {
        return wmbus_rx_send_event(ctx, WmBusRxViewEventToggleDebug);
    }

    if(event->type == InputTypeLong && event->key == InputKeyDown) {
        return wmbus_rx_send_event(ctx, WmBusRxViewEventOpenDetails);
    }

    return false;
}

WmBusRxView* wmbus_rx_view_alloc(void) {
    WmBusRxView* rx_view = malloc(sizeof(WmBusRxView));
    rx_view->view = view_alloc();
    view_allocate_model(rx_view->view, ViewModelTypeLocking, sizeof(WmBusRxViewModel));

    with_view_model(
        rx_view->view,
        WmBusRxViewModel * model,
        {
            memset(model, 0, sizeof(*model));
            model->freq_valid = true;
            model->mode = WmBusRxModeT;
        },
        false);

    view_set_context(rx_view->view, &rx_view->context);
    rx_view->context.view = rx_view->view;
    view_set_draw_callback(rx_view->view, wmbus_rx_draw);
    view_set_input_callback(rx_view->view, wmbus_rx_input);
    return rx_view;
}

void wmbus_rx_view_free(WmBusRxView* rx_view) {
    if(!rx_view) return;
    view_free(rx_view->view);
    free(rx_view);
}

View* wmbus_rx_view_get_view(WmBusRxView* rx_view) {
    return rx_view ? rx_view->view : NULL;
}

void wmbus_rx_view_set_dispatcher(WmBusRxView* rx_view, ViewDispatcher* view_dispatcher) {
    if(!rx_view) return;
    rx_view->context.view_dispatcher = view_dispatcher;
}

void wmbus_rx_view_apply_settings(WmBusRxView* rx_view, const WmBusSettings* settings) {
    if(!rx_view || !settings) return;
    with_view_model(
        rx_view->view,
        WmBusRxViewModel * model,
        {
            model->mode = settings->mode;
            model->debug_mode = settings->debug_overlay;
        },
        true);
}

void wmbus_rx_view_set_freq_valid(WmBusRxView* rx_view, bool freq_valid) {
    if(!rx_view) return;
    with_view_model(
        rx_view->view, WmBusRxViewModel * model, { model->freq_valid = freq_valid; }, true);
}

void wmbus_rx_view_set_live_rssi(WmBusRxView* rx_view, int rssi) {
    if(!rx_view) return;
    with_view_model(rx_view->view, WmBusRxViewModel * model, { model->rssi = rssi; }, true);
}

static void wmbus_rx_rssi_hist_push(WmBusRxViewModel* model, int rssi) {
    uint8_t next = (model->rssi_hist_count == 0U) ?
                       0U :
                       (uint8_t)((model->rssi_hist_head + 1U) % WMBUS_RSSI_HISTORY);
    model->rssi_hist_head = next;
    if(model->rssi_hist_count < WMBUS_RSSI_HISTORY) {
        model->rssi_hist_count++;
    }
    model->rssi_hist[model->rssi_hist_head] = (int8_t)rssi;
}

static void wmbus_rx_history_fill_entry(
    WmBusRxHistoryEntry* entry,
    const WmBusPacketRecord* record) {
    if(!entry || !record) return;

    memset(entry, 0, sizeof(*entry));
    entry->packet_is_frame = record->packet_is_frame;
    entry->status = record->status;
    entry->mode = record->mode;
    entry->rssi = (int8_t)record->rssi;
    entry->rx_tick = record->rx_tick;
    entry->packet_len = record->packet_len;
    entry->packet_preview_len = (uint8_t)((record->packet_len > WMBUS_RX_DEBUG_PACKET_MAX) ?
                                              WMBUS_RX_DEBUG_PACKET_MAX :
                                              record->packet_len);
    memcpy(entry->packet_preview, record->packet_bytes, entry->packet_preview_len);
    entry->dll = record->dll;
    entry->identity = record->identity;
    entry->ell = record->ell;
    entry->tpl = record->tpl;
    entry->application = record->application;
}

static void wmbus_rx_history_entry_to_record(
    const WmBusRxHistoryEntry* entry,
    WmBusPacketRecord* record) {
    if(!entry || !record) {
        return;
    }

    memset(record, 0, sizeof(*record));
    record->packet_is_frame = entry->packet_is_frame;
    record->status = entry->status;
    record->mode = entry->mode;
    record->rssi = entry->rssi;
    record->rx_tick = entry->rx_tick;
    record->packet_len = entry->packet_len;
    record->dll = entry->dll;
    record->identity = entry->identity;
    record->ell = entry->ell;
    record->tpl = entry->tpl;
    record->application = entry->application;
}

void wmbus_rx_view_push_packet(
    WmBusRxView* rx_view,
    const WmBusPacketRecord* record,
    bool store_in_history) {
    if(!rx_view || !record) return;

    with_view_model(
        rx_view->view,
        WmBusRxViewModel * model,
        {
            model->rssi = record->rssi;
            model->last_status = record->status;
            wmbus_rx_history_fill_entry(&model->latest, record);
            model->has_latest = true;

            if(record->strong_rssi) {
                model->packets_strong++;
            }

            if(record->plausible) {
                model->packets_decoded++;
                if(record->crc_known) {
                    if(record->crc_ok) {
            model->packets_crc_ok++;
                    } else {
                        model->packets_crc_bad++;
                    }
                }
                model->last_crc_valid = record->crc_known;
                model->last_crc_ok = record->crc_ok;
            } else {
                model->last_crc_valid = false;
                model->last_crc_ok = false;
            }

            wmbus_rx_rssi_hist_push(model, record->rssi);

            if(store_in_history) {
                uint8_t next = (model->hist_count == 0U) ?
                                   0U :
                                   (uint8_t)((model->hist_head + 1U) % WMBUS_HIST_MAX);
                model->hist_head = next;
                if(model->hist_count < WMBUS_HIST_MAX) {
                    model->hist_count++;
                }

                WmBusRxHistoryEntry* entry = &model->hist[model->hist_head];
                wmbus_rx_history_fill_entry(entry, record);

                if(model->freeze_display) {
                    if(model->hist_count > 0U && model->hist_cursor + 1U < model->hist_count) {
                        model->hist_cursor++;
                    }
                } else {
                    model->hist_cursor = 0U;
                }
            }
        },
        true);
}

bool wmbus_rx_view_has_selected_packet(WmBusRxView* rx_view) {
    if(!rx_view) return false;

    bool has_packet = false;
    with_view_model(
        rx_view->view,
        WmBusRxViewModel * model,
        { has_packet = (wmbus_rx_display_entry_get(model) != NULL); },
        false);
    return has_packet;
}

bool wmbus_rx_view_build_selected_detail_text(WmBusRxView* rx_view, FuriString* out) {
    if(!rx_view || !out) return false;
    furi_string_reset(out);

    bool found = false;
    with_view_model(
        rx_view->view,
        WmBusRxViewModel * model,
        {
            const WmBusRxHistoryEntry* entry = wmbus_rx_display_entry_get(model);
            if(entry) {
                WmBusPacketRecord record = {0};
                wmbus_rx_history_entry_to_record(entry, &record);
                wmbus_packet_format_detail_text(&record, out);
                found = true;
            }
        },
        false);
    return found;
}
