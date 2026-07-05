#pragma once

#include <furi.h>

#include "../packet/wmbus_packet.h"

/**
 * Format packet detail text for UI or logging.
 * Resets `out`. Empty output is valid only when `record` is `NULL`.
 * The caller owns `out`.
 */
void wmbus_packet_format_detail_text(const WmBusPacketRecord* record, FuriString* out);
