#include "wmbus_packet_summary.h"

#include <stdio.h>

#include "../model/wmbus_application_record.h"
#include "../parser/wmbus_parser.h"

bool wmbus_packet_summary_find_total_m3(
    const WmBusPacketApplicationData* application,
    uint32_t* total_m3_x1000) {
    if(!application) return false;
    return wmbus_application_find_total_volume(
        application->records, application->record_count, total_m3_x1000);
}

void wmbus_packet_summary_format_total_m3(
    uint32_t total_m3_x1000,
    char* out,
    size_t out_size,
    bool with_unit) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';

    uint32_t whole = total_m3_x1000 / 1000U;
    uint32_t frac = total_m3_x1000 % 1000U;
    snprintf(
        out,
        out_size,
        with_unit ? "%lu.%03lu m3" : "%lu.%03lu",
        (unsigned long)whole,
        (unsigned long)frac);
}

static const char* wmbus_packet_summary_security_mode_name(uint8_t security_mode) {
    switch(security_mode) {
    case 0x00:
        return "Clear";
    case 0x01:
        return "Manufacturer";
    case 0x05:
        return "AES-CBC IV";
    case 0x08:
        return "AES-CTR CMAC";
    default:
        return NULL;
    }
}

static const char* wmbus_packet_summary_ell_security_mode_name(uint8_t security_mode) {
    switch(security_mode) {
    case 0x00:
        return "Clear";
    case 0x01:
        return "AES-CTR";
    default:
        return NULL;
    }
}

void wmbus_packet_summary_format_crypto_tag(
    const WmBusPacketEllData* ell,
    const WmBusPacketTplData* tpl,
    char* out,
    size_t out_size) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';

    if(ell && ell->has_ell && ell->has_session) {
        if(ell->decrypted) {
            if(ell->key_index != 0U) {
                snprintf(out, out_size, "EDEC#%u", (unsigned int)ell->key_index);
            } else {
                snprintf(out, out_size, "EDEC0");
            }
        } else if(wmbus_parser_ell_security_likely_encrypted(ell->sn)) {
            snprintf(out, out_size, "EENC");
        } else if(ell->security_mode == 0x00U) {
            snprintf(out, out_size, "ECLR");
        } else {
            snprintf(out, out_size, "ES:%02X", ell->security_mode);
        }
        return;
    }

    if(!tpl || !tpl->has_short_tpl) return;

    if(tpl->decrypted) {
        if(tpl->key_index != 0U) {
            snprintf(out, out_size, "DEC#%u", (unsigned int)tpl->key_index);
        } else {
            snprintf(out, out_size, "DEC0");
        }
    } else if(wmbus_parser_short_tpl_security_likely_encrypted(tpl->cfg)) {
        snprintf(out, out_size, "ENC");
    } else if(tpl->security_mode == 0x00U) {
        snprintf(out, out_size, "CLR");
    } else if(tpl->security_mode == 0x01U) {
        snprintf(out, out_size, "MFG");
    } else {
        snprintf(out, out_size, "S:%02X", tpl->security_mode);
    }
}

void wmbus_packet_summary_format_security_text(
    const WmBusPacketEllData* ell,
    const WmBusPacketTplData* tpl,
    char* out,
    size_t out_size) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';

    if(ell && ell->has_ell) {
        if(!ell->has_session) {
            snprintf(out, out_size, "ELL");
            return;
        }

        char mode[20] = {0};
        const char* known_mode = wmbus_packet_summary_ell_security_mode_name(ell->security_mode);
        if(known_mode) {
            snprintf(mode, sizeof(mode), "%s", known_mode);
        } else {
            snprintf(mode, sizeof(mode), "Mode %02X", ell->security_mode);
        }

        if(ell->decrypted) {
            if(ell->key_index != 0U) {
                snprintf(out, out_size, "ELL %s, decrypted key #%u", mode, (unsigned int)ell->key_index);
            } else {
                snprintf(out, out_size, "ELL %s, decrypted zero key", mode);
            }
        } else if(wmbus_parser_ell_security_likely_encrypted(ell->sn)) {
            snprintf(out, out_size, "ELL %s, encrypted", mode);
        } else {
            snprintf(out, out_size, "ELL %s", mode);
        }
        return;
    }

    if(!tpl || !tpl->has_short_tpl) return;

    char mode[20] = {0};
    const char* known_mode = wmbus_packet_summary_security_mode_name(tpl->security_mode);
    if(known_mode) {
        snprintf(mode, sizeof(mode), "%s", known_mode);
    } else {
        snprintf(mode, sizeof(mode), "Mode %02X", tpl->security_mode);
    }

    if(tpl->decrypted) {
        if(tpl->key_index != 0U) {
            snprintf(out, out_size, "%s, decrypted key #%u", mode, (unsigned int)tpl->key_index);
        } else {
            snprintf(out, out_size, "%s, decrypted zero key", mode);
        }
    } else if(wmbus_parser_short_tpl_security_likely_encrypted(tpl->cfg)) {
        snprintf(out, out_size, "%s, encrypted", mode);
    } else {
        snprintf(out, out_size, "%s", mode);
    }
}

void wmbus_packet_summary_format_bottom_line(
    bool packet_is_frame,
    uint16_t packet_len,
    const WmBusPacketDllData* dll,
    const WmBusPacketEllData* ell,
    const WmBusPacketTplData* tpl,
    bool has_total_volume,
    uint32_t total_m3_x1000,
    char* out,
    size_t out_size) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';

    char crypto[12] = {0};
    wmbus_packet_summary_format_crypto_tag(ell, tpl, crypto, sizeof(crypto));

    if(has_total_volume) {
        uint32_t whole = total_m3_x1000 / 1000U;
        uint32_t frac = total_m3_x1000 % 1000U;
        if(crypto[0] != '\0') {
            snprintf(
                out,
                out_size,
                "Vol:%lu.%03lum3 %s",
                (unsigned long)whole,
                (unsigned long)frac,
                crypto);
        } else {
            snprintf(
                out, out_size, "Vol:%lu.%03lum3", (unsigned long)whole, (unsigned long)frac);
        }
    } else if(packet_is_frame && dll) {
        if(crypto[0] != '\0') {
            snprintf(out, out_size, "CI:%02X %s", dll->ci_field, crypto);
        } else {
            snprintf(out, out_size, "CI:%02X", dll->ci_field);
        }
    } else {
        snprintf(out, out_size, "Len:%u bytes", (unsigned int)packet_len);
    }
}
