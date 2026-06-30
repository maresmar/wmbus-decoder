#include "wmbus_packet_summary.h"

#include <stdio.h>

#include "../parser/wmbus_parser.h"

static const char*
    wmbus_packet_summary_security_mode_name(uint8_t security_mode, bool ell_security) {
    switch(security_mode) {
    case 0x00:
        return "Clear";
    case 0x01:
        return ell_security ? "AES-CTR" : "Manufacturer";
    case 0x05:
        return ell_security ? NULL : "AES-CBC IV";
    case 0x08:
        return ell_security ? NULL : "AES-CTR CMAC";
    default:
        return NULL;
    }
}

static void wmbus_packet_summary_format_security_mode(
    uint8_t security_mode,
    bool ell_security,
    char* out,
    size_t out_size) {
    if(!out || out_size == 0U) return;

    const char* known_mode =
        wmbus_packet_summary_security_mode_name(security_mode, ell_security);
    if(known_mode) {
        snprintf(out, out_size, "%s", known_mode);
    } else {
        snprintf(out, out_size, "Mode %02X", security_mode);
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
            snprintf(out, out_size, "EDEC#%u", (unsigned int)ell->key_index);
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
        snprintf(out, out_size, "DEC#%u", (unsigned int)tpl->key_index);
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
        wmbus_packet_summary_format_security_mode(ell->security_mode, true, mode, sizeof(mode));

        if(ell->decrypted) {
            snprintf(out, out_size, "ELL %s, decrypted key #%u", mode, (unsigned int)ell->key_index);
        } else if(wmbus_parser_ell_security_likely_encrypted(ell->sn)) {
            snprintf(out, out_size, "ELL %s, encrypted", mode);
        } else {
            snprintf(out, out_size, "ELL %s", mode);
        }
        return;
    }

    if(!tpl || !tpl->has_short_tpl) return;

    char mode[20] = {0};
    wmbus_packet_summary_format_security_mode(tpl->security_mode, false, mode, sizeof(mode));

    if(tpl->decrypted) {
        snprintf(out, out_size, "%s, decrypted key #%u", mode, (unsigned int)tpl->key_index);
    } else if(wmbus_parser_short_tpl_security_likely_encrypted(tpl->cfg)) {
        snprintf(out, out_size, "%s, encrypted", mode);
    } else {
        snprintf(out, out_size, "%s", mode);
    }
}
