# WM-Bus Decoder

`wmbus_decoder` is a receive-only Flipper Zero Sub-GHz app for Wireless M-Bus at `868.95 MHz`.

## Supported Features

- WM-Bus receive pipeline with `T` and experimental `C` mode capture paths
- packet validation with plausibility, length, and CRC checks
- packet history on device with quality-based retention threshold and optional RSSI gate
- optional dated CSV logging with quality-based CSV threshold
- optional AES key loading from `keys.txt`
- generic DIF/VIF parsing for standard application records
- targeted device parsing for supported telegram families
- startup selftest report written to `/ext/apps_data/wmbus_decoder/selftest.txt`

### Packet flow

1. radio RX captures raw telegram data in `T` or `C` mode
2. capture code reconstructs a candidate WM-Bus frame for the selected mode
3. packet processing validates plausibility, length, and CRC
4. packet metadata, identity fields, and payload slices are stored in `WmBusPacketRecord`
5. optional decrypt attempts run before application parsing when the packet indicates supported security
6. registered parsers inspect the packet and populate application-level fields
7. the result is routed to live view, history storage, and optional CSV logging
8. history and CSV retention apply optional RSSI and packet-quality gates

## Current Limitations

- `C` mode exists, but is not considered fully validated on real hardware
- the app is receive-only; it does not transmit or pair
- device-specific parsing is limited; unsupported telegrams fall back to generic decode or remain undecoded
- decryption support is limited to what the shared packet path and registered parsers can validate

## Parsers

Currently registered application parsers:

- `Apator162`: targeted device parser for supported Apator short-TPL telegrams
- `DifVif`: generic parser for standard DIF/VIF application records

Parser registration is order-dependent. The first parser whose `probe()` and `parse()` both succeed wins, so device-specific parsers must stay ahead of generic fallbacks such as `DifVif`.

## Adding A Parser

New parsers live under `applications_user/wmbus_decoder/protocol/parser/`.

Minimum steps:

1. add a new parser ID in `wmbus_parser_id.h`
2. add a parser implementation with `probe(const WmBusParserPacketView*)` and `parse(const WmBusParserPacketView*, WmBusPacketApplicationData*)`
3. include the new parser in `wmbus_device_parser.c`
4. register it in `wmbus_device_parsers[]` with `parser_id`, display `name`, and the correct `validates_decrypt` flag
5. place it before any broader parser it should override
6. add or update selftests in `applications_user/wmbus_decoder/test/`

Parser inputs come through `WmBusParserPacketView`, which exposes DLL, TPL, payload, and identity data from the shared packet pipeline. New parser tests should exercise the full packet path through `wmbus_packet_process_capture(...)` rather than calling parser internals directly.

## Runtime Controls

- `OK`: switch between `Latest` and `History`
- `Up` / `Down`: browse saved packets in `History`
- `Left`: open config
- `Right`: open packet detail for the selected history entry
- `Back`: exit

## Configuration

Available settings:

- RX mode: `T` / `C`
- CSV logging: `Off` / `Basic` / `Full`
- memory quality threshold: `MEM gate >=`
- CSV quality threshold: `CSV gate >=`
- RSSI gate: `Off` or a negative dBm threshold
- keyring status and key entry

Files used by the app:

- settings: `/ext/apps_data/wmbus_decoder/settings.txt`
- keys: `/ext/apps_data/wmbus_decoder/keys.txt`
- selftest report: `/ext/apps_data/wmbus_decoder/selftest.txt`
- CSV basic: `/ext/apps_data/wmbus_decoder/packets_YYYYMMDD_basic.csv`
- CSV full: `/ext/apps_data/wmbus_decoder/packets_YYYYMMDD_full.csv`

## Key File

Optional AES keys are loaded from:

`/ext/apps_data/wmbus_decoder/keys.txt`

Format:

```text
00112233445566778899AABBCCDDEEFF
```

- one AES-128 key per line as 32 hex characters
- lines starting with `#` are ignored
- keys are tried in file order

## Build

From this app directory:

```bash
ufbt
```

Output:

`dist/wmbus_decoder.fap`

## Tests

The app runs its selftests at startup and writes the report to:

`/ext/apps_data/wmbus_decoder/selftest.txt`

After building and launching the app on a device, fetch the report with the Flipper CLI:

```bash
ufbt cli
```

Then run:

```text
storage read /ext/apps_data/wmbus_decoder/selftest.txt
```

Expected result ends with:

```text
selftests done total=58 passed=58 failed=0
```

## Credits

- Inspired by [wmbusmeters](https://wmbusmeters.org/)
