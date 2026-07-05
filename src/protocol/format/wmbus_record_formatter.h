#pragma once

#include <furi.h>

#include "../model/wmbus_application_types.h"

/**
 * Format one application record as `label=value`.
 * Resets `out`. Empty output is valid when either side is not printable.
 * The caller owns `out`.
 */
bool wmbus_record_formatter_format_field(const WmBusApplicationRecord* record, FuriString* out);

/**
 * Format multiple application records separated by `delimiter`.
 * Resets `out`. Empty output is valid when no record produces text.
 * The caller owns `out`.
 */
bool wmbus_record_formatter_format_joined(
    const WmBusApplicationRecord* records,
    uint8_t record_count,
    char delimiter,
    FuriString* out);
