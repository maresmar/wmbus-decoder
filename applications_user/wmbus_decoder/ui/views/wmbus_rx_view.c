#include "wmbus_rx_view.h"

#include "../../app/wmbus_format.h"

#include <furi.h>
#include <gui/canvas.h>
#include <input/input.h>
#include <stdio.h>
#include <string.h>

#define WMBUS_HIST_MAX          100U
#define WMBUS_RSSI_HISTORY      64U
#define WMBUS_RX_PRIMARY_MAX    24U
#define WMBUS_RX_FIELD_TEXT_MAX 96U
#define WMBUS_RX_PREVIEW_MAX    16U

typedef struct {
    bool packet_is_frame;
    uint8_t l_field;
    uint8_t c_field;
    char mfg[4];
    char id_str[9];
    uint8_t version;
    uint8_t dev_type;
    uint8_t ci_field;
    int8_t rssi;
    uint32_t rx_tick;
    WmBusStatus status;
    bool crc_ok;
    bool used_3of6;
    bool has_short_tpl;
    uint8_t security_mode;
    bool security_likely_encrypted;
    bool decrypted;
    uint8_t key_index;
    char parser_name[WMBUS_PACKET_PARSER_NAME_MAX];
    bool has_total_m3;
    uint32_t total_m3_x1000;
    char field_text[WMBUS_RX_FIELD_TEXT_MAX];
    uint8_t packet_len;
    uint8_t packet_preview[WMBUS_RX_PREVIEW_MAX];
} WmBusRxHistoryEntry;

typedef struct {
    ViewDispatcher* view_dispatcher;
    View* view;
} WmBusRxViewContext;

typedef struct {
    bool freq_valid;
    bool debug_mode;
    WmBusRxMode mode;
    int rssi;
    WmBusStatus last_status;
    uint32_t rate_last_tick;
    uint32_t rate_last_seen;
    uint16_t packets_per_sec;

    uint32_t packets_seen;
    uint32_t packets_decoded;
    uint32_t packets_strong;
    uint32_t packets_crc_ok;
    uint32_t packets_crc_bad;
    bool last_crc_valid;
    bool last_crc_ok;

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

static const char* wmbus_rx_mode_label(WmBusRxMode mode) {
    return (mode == WmBusRxModeC) ? "C" : "T";
}

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
    wmbus_rx_format_crypto_tag(const WmBusRxHistoryEntry* entry, char* out, size_t out_size) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';
    if(!entry || !entry->has_short_tpl) return;

    if(entry->decrypted) {
        if(entry->key_index != 0U) {
            snprintf(out, out_size, "DEC#%u", (unsigned int)entry->key_index);
        } else {
            snprintf(out, out_size, "DEC0");
        }
    } else if(entry->security_likely_encrypted) {
        snprintf(out, out_size, "ENC");
    } else if(entry->security_mode == 0x00U) {
        snprintf(out, out_size, "CLR");
    } else if(entry->security_mode == 0x01U) {
        snprintf(out, out_size, "MFG");
    } else {
        snprintf(out, out_size, "S:%02X", entry->security_mode);
    }
}

static bool wmbus_rx_parser_name_is_generic(const char* parser_name) {
    if(!parser_name || parser_name[0] == '\0') return true;

    return strcmp(parser_name, "Short TPL") == 0 || strcmp(parser_name, "Header") == 0 ||
           strcmp(parser_name, "Raw") == 0 || strcmp(parser_name, "DIF/VIF") == 0;
}

static void
    wmbus_rx_format_bottom_line(const WmBusRxHistoryEntry* entry, char* out, size_t out_size) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';
    if(!entry) return;

    char crypto[12] = {0};
    wmbus_rx_format_crypto_tag(entry, crypto, sizeof(crypto));

    if(entry->has_total_m3) {
        uint32_t whole = entry->total_m3_x1000 / 1000U;
        uint32_t frac = entry->total_m3_x1000 % 1000U;
        if(crypto[0] != '\0') {
            snprintf(
                out,
                out_size,
                "M:%c R:%d C:%s %lu.%03lum3",
                entry->used_3of6 ? 'T' : 'C',
                entry->rssi,
                crypto,
                (unsigned long)whole,
                (unsigned long)frac);
        } else {
            snprintf(
                out,
                out_size,
                "M:%c R:%d %lu.%03lum3",
                entry->used_3of6 ? 'T' : 'C',
                entry->rssi,
                (unsigned long)whole,
                (unsigned long)frac);
        }
    } else if(entry->packet_is_frame) {
        if(crypto[0] != '\0') {
            snprintf(
                out,
                out_size,
                "M:%c R:%d %s CI:%02X",
                entry->used_3of6 ? 'T' : 'C',
                entry->rssi,
                crypto,
                entry->ci_field);
        } else {
            snprintf(
                out,
                out_size,
                "M:%c R:%d CI:%02X",
                entry->used_3of6 ? 'T' : 'C',
                entry->rssi,
                entry->ci_field);
        }
    } else {
        snprintf(
            out,
            out_size,
            "M:%c R:%d Len:%u",
            entry->used_3of6 ? 'T' : 'C',
            entry->rssi,
            (unsigned int)entry->packet_len);
    }
}

static void wmbus_rx_draw(Canvas* canvas, void* model) {
    WmBusRxViewModel* m = model;
    char line[64];
    char age[12];
    char header[16];

    uint8_t display_cursor = m->hist_cursor;
    if(display_cursor >= m->hist_count && !m->freeze_display && m->hist_count > 0U) {
        display_cursor = (uint8_t)(m->hist_count - 1U);
    }
    const WmBusRxHistoryEntry* entry = wmbus_rx_history_get(m, display_cursor);

    if(m->freeze_display) {
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
        "Lst M:%s R:%u/s RSSI:%d",
        wmbus_rx_mode_label(m->mode),
        (unsigned int)m->packets_per_sec,
        m->rssi);
    canvas_draw_str(canvas, 0, 28, line);

    snprintf(
        line,
        sizeof(line),
        "%s %s",
        m->freeze_display ? "Pkt" : "Last",
        wmbus_packet_status_str(entry ? entry->status : m->last_status));
    canvas_draw_str(canvas, 0, 38, line);
    if(age[0] != '\0') {
        snprintf(line, sizeof(line), "A:%s", age);
        canvas_draw_str_aligned(canvas, canvas_width(canvas), 38, AlignRight, AlignBottom, line);
    }

    if(!entry) {
        canvas_draw_str(canvas, 0, 48, "Waiting for RX...");
        canvas_draw_str(canvas, 0, 58, "OK=history  long OK=config");
    } else if(m->debug_mode) {
        char hex[WMBUS_PACKET_VALUE_MAX];
        wmbus_rx_preview_hex(entry->packet_preview, entry->packet_len, hex, sizeof(hex));
        snprintf(
            line,
            sizeof(line),
            "L:%02X C:%02X CI:%02X V:%02X",
            entry->l_field,
            entry->c_field,
            entry->ci_field,
            entry->version);
        canvas_draw_str(canvas, 0, 48, line);
        snprintf(line, sizeof(line), "Hex:%s%s", hex, (entry->packet_len > 8U) ? "..." : "");
        canvas_draw_str(canvas, 0, 58, line);
    } else {
        char right[20];
        snprintf(line, sizeof(line), "MF:%s DT:%02X", entry->mfg, entry->dev_type);
        canvas_draw_str(canvas, 0, 48, line);
        snprintf(right, sizeof(right), "ID:%s", entry->id_str);
        canvas_draw_str_aligned(canvas, canvas_width(canvas), 48, AlignRight, AlignBottom, right);
        wmbus_rx_format_bottom_line(entry, line, sizeof(line));
        canvas_draw_str(canvas, 0, 58, line);
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

void wmbus_rx_view_push_packet(
    WmBusRxView* rx_view,
    const WmBusPacketRecord* record,
    bool store_in_history) {
    if(!rx_view || !record) return;

    with_view_model(
        rx_view->view,
        WmBusRxViewModel * model,
        {
            model->packets_seen++;
            model->rssi = record->rssi;
            model->last_status = record->status;

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

            uint32_t tick_freq = furi_kernel_get_tick_frequency();
            if(model->rate_last_tick == 0U) {
                model->rate_last_tick = record->rx_tick;
                model->rate_last_seen = model->packets_seen;
                model->packets_per_sec = 0U;
            } else if((record->rx_tick - model->rate_last_tick) >= tick_freq) {
                uint32_t elapsed = record->rx_tick - model->rate_last_tick;
                uint32_t diff = model->packets_seen - model->rate_last_seen;
                model->packets_per_sec = (uint16_t)((diff * tick_freq) / (elapsed ? elapsed : 1U));
                model->rate_last_tick = record->rx_tick;
                model->rate_last_seen = model->packets_seen;
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
                memset(entry, 0, sizeof(*entry));
                entry->packet_is_frame = record->packet_is_frame;
                entry->l_field = record->frame.l_field;
                entry->c_field = record->frame.c_field;
                snprintf(entry->mfg, sizeof(entry->mfg), "%s", record->frame.mfg);
                snprintf(entry->id_str, sizeof(entry->id_str), "%s", record->frame.id_str);
                entry->version = record->frame.version;
                entry->dev_type = record->frame.dev_type;
                entry->ci_field = record->frame.ci_field;
                entry->rssi = (int8_t)record->rssi;
                entry->rx_tick = record->rx_tick;
                entry->status = record->status;
                entry->crc_ok = record->crc_ok;
                entry->used_3of6 = (record->mode == WmBusRxModeT);
                entry->has_short_tpl = record->transport.has_short_tpl;
                entry->security_mode = record->transport.security_mode;
                entry->security_likely_encrypted = record->transport.security_likely_encrypted;
                entry->decrypted = record->transport.decrypted;
                entry->key_index = record->transport.key_index;
                snprintf(
                    entry->parser_name,
                    sizeof(entry->parser_name),
                    "%.*s",
                    (int)(sizeof(entry->parser_name) - 1U),
                    record->application.parser_name);
                entry->has_total_m3 =
                    wmbus_format_find_total_volume(record, &entry->total_m3_x1000);
                wmbus_format_fields_text(record, entry->field_text, sizeof(entry->field_text));
                entry->packet_len = (uint8_t)((record->packet_len > WMBUS_RX_PREVIEW_MAX) ?
                                                  WMBUS_RX_PREVIEW_MAX :
                                                  record->packet_len);
                memcpy(entry->packet_preview, record->packet_bytes, entry->packet_len);

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
        { has_packet = (wmbus_rx_history_get(model, model->hist_cursor) != NULL); },
        false);
    return has_packet;
}

bool wmbus_rx_view_build_selected_detail_text(WmBusRxView* rx_view, char* out, size_t out_size) {
    if(!rx_view || !out || out_size == 0U) return false;
    out[0] = '\0';

    bool found = false;
    with_view_model(
        rx_view->view,
        WmBusRxViewModel * model,
        {
            const WmBusRxHistoryEntry* entry = wmbus_rx_history_get(model, model->hist_cursor);
            if(entry) {
                char total[WMBUS_PACKET_VALUE_MAX] = {0};
                char security[48] = {0};
                char total_line[48] = {0};
                char security_line[64] = {0};
                char detail_fields[160] = {0};
                char parser_line[40] = {0};
                char mode_line[32] = {0};
                const char* detail_tail = "-";
                if(entry->has_total_m3) {
                    wmbus_packet_format_total_m3(entry->total_m3_x1000, total, sizeof(total));
                    snprintf(total_line, sizeof(total_line), "Total: %s\n", total);
                }
                wmbus_packet_format_security_text(
                    entry->has_short_tpl,
                    entry->security_mode,
                    entry->security_likely_encrypted,
                    entry->decrypted,
                    entry->key_index,
                    security,
                    sizeof(security));
                if(security[0]) {
                    snprintf(security_line, sizeof(security_line), "Security: %s\n", security);
                }
                if(entry->field_text[0]) {
                    snprintf(
                        detail_fields, sizeof(detail_fields), "Fields: %s", entry->field_text);
                }
                if(detail_fields[0]) {
                    detail_tail = detail_fields;
                } else if(total_line[0] || security_line[0]) {
                    detail_tail = "";
                }

                snprintf(
                    mode_line,
                    sizeof(mode_line),
                    "M:%c  R:%d",
                    entry->used_3of6 ? 'T' : 'C',
                    entry->rssi);
                if(!wmbus_rx_parser_name_is_generic(entry->parser_name)) {
                    snprintf(parser_line, sizeof(parser_line), "Parser: %s\n", entry->parser_name);
                }

                if(entry->packet_is_frame) {
                    snprintf(
                        out,
                        out_size,
                        "Status: %s\nMF:%s  DT:%02X  ID:%s\n%s\nCI:%02X  V:%02X\n%s%s%s%s",
                        wmbus_packet_status_str(entry->status),
                        entry->mfg,
                        entry->dev_type,
                        entry->id_str,
                        mode_line,
                        entry->ci_field,
                        entry->version,
                        parser_line,
                        total_line,
                        security_line,
                        detail_tail);
                } else {
                    snprintf(
                        out,
                        out_size,
                        "Status: %s\nMode: %c  RSSI: %d\nParser: %s\nLen: %u bytes",
                        wmbus_packet_status_str(entry->status),
                        entry->used_3of6 ? 'T' : 'C',
                        entry->rssi,
                        entry->parser_name,
                        (unsigned int)entry->packet_len);
                }
                found = true;
            }
        },
        false);
    return found;
}
