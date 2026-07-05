#pragma once

/*
 * Parser identifiers are owned by protocol/parser.
 *
 * They are referenced by packet/application records, formatters, tests, and UI,
 * but new IDs should be introduced from the parser layer rather than model code.
 */
typedef enum {
    WmBusParserIdUnknown = 0,
    WmBusParserIdRaw,
    WmBusParserIdHeader,
    WmBusParserIdEll,
    WmBusParserIdShortTpl,
    WmBusParserIdDifVif,
    WmBusParserIdApator162,
} WmBusParserId;
