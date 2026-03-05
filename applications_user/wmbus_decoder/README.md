# WM-Bus Decoder (Flipper Zero)

Developer-focused notes for `applications_user/wmbus_decoder`.

## Copilot Instructions

Use this as startup context for future coding chats in this app.

- Scope:
  - Work only inside `applications_user/wmbus_decoder/` unless explicitly requested.
  - Preserve manual mode control (`Left=T`, `Right=C`).
- Target meter:
  - Primary target is Apator `AT-WMBUS-16-2` (`apator162`).
  - Proprietary payload support is intentionally limited to total volume extraction (`total_m3_x1000`).
  - Old Apator style with `CI=0xB6` is out of scope unless asked.
- RX architecture constraints:
  - RX control is queue-driven (`FuriMessageQueue`) using `WmBusControlCmd`.
  - Do not reintroduce shared volatile mode/running flags across threads.
  - Keep plausibility gate for WM-Bus `C-field` valid values (`0x44`, `0x46`).
  - Keep frame length trimming before CRC/model updates.
- Data model/UI:
  - `WmBusViewModel` and `WmBusHistoryEntry` include `has_total_m3` and `total_m3_x1000`.
  - Normal view should show `Tot:<whole>.<frac>m3` when available.
  - Debug mode remains header/hex oriented.
- Build/verify commands:
  - Preferred build: `CCACHE_DISABLE=1 ./fbt fap_wmbus_decoder`
  - If parser changes are made, validate against known Apator vectors from
    `wmbusmeters/src/driver_apator162.cc` (expected totals: `3.843`, `270.133`, `30.908` m3).
- Editing preferences:
  - Keep changes minimal and localized.
  - Avoid broad refactors unless requested.
  - Update this README when behavior, controls, or decoder assumptions change.

## Purpose

`wmbus_decoder` is a Sub-GHz receive-only app for Wireless M-Bus at `868.95 MHz`.
It focuses on:

- link-layer RX quality (frame plausibility, length and CRC checks)
- quick field visibility on-device (manufacturer, meter ID, CI, RSSI)
- targeted payload extraction for Apator `AT-WMBUS-16-2` (`apator162`) total volume

## Build and Install

From firmware root:

```bash
./fbt fap_wmbus_decoder
```

Output artifact:

- `build/f7-firmware-D/.extapps/wmbus_decoder.fap`

Deploy as you normally deploy external apps in your Flipper firmware workflow.

## Tests (Flipper Unit Tests)

This app follows Flipper's built-in `applications/debug/unit_tests` plugin model.

- test plugin id: `test_wmbus_decoder`
- test source: `applications/debug/unit_tests/tests/wmbus_decoder/wmbus_decoder_test.c`
- parser under test: `applications_user/wmbus_decoder/wmbus_parser.c`

Build tests:

```bash
./fbt FIRMWARE_APP_SET=unit_tests faps
```

Run on device (CLI):

```bash
unit_tests test_wmbus_decoder
```

Covered scenarios:

- 3-of-6 decode success/failure edges
- WM-Bus plausibility gates (`L-field`, `C-field`, manufacturer)
- Apator162 register size map sanity
- Apator162 total-volume extraction from known vectors
- rejection of unsupported old-style CI (`0xB6`)

## Runtime Controls

- `Left`: force `T` mode (3-of-6 path)
- `Right`: force `C` mode (whitened direct-byte path)
- `OK`: freeze/unfreeze history cursor
- `Up`/`Down`: browse history
- `Long Up`: toggle debug overlay
- `Back`: exit app

Mode commands are delivered to RX thread through a `FuriMessageQueue`, avoiding racy shared state.

## RX/Decode Architecture

### Radio

- CC1101 custom preset for WM-Bus receive at `868.95 MHz`
- mode-dependent sync/whitening switch (`T` vs `C`)

### Packet processing

1. Read bytes from Sub-GHz RX FIFO.
2. Estimate expected frame length from `L-field` when possible.
3. Complete frame on timeout, full buffer, or expected-length reached.
4. Decode path:
- `T`: 3-of-6 decode into byte stream.
- `C`: direct byte stream.
5. Plausibility gates:
- minimum header length
- valid `L-field`
- valid WM-Bus `C-field` (`0x44` or `0x46`)
- valid manufacturer code shape
6. Length + EN13757 CRC checks.
7. Update view model/history and confidence counters.

### Frame format handling

- `T` path uses frame format A length logic.
- `C` path uses frame format B length logic.
- Parsed `frame_len` is trimmed to expected length before CRC/model updates.

## Apator `AT-WMBUS-16-2` Parsing

A targeted parser extracts total volume from proprietary register payloads.

Implemented behavior:

- requires `CI == 0x7A`
- starts after short TPL header
- skips leading `0x2F` fillers
- skips first 8 vendor bytes
- iterates vendor registers using `apator162` register-size table
- extracts register `0x10` or `0xA1` as little-endian `uint32`
- interprets raw value as `m3 * 1000`

Displayed on normal UI when available:

- `Tot:<whole>.<frac>m3`

Known limit (intentional):

- old-style Apator telegrams (`CI == 0xB6`) are not decoded.

## References

- `wmbusmeters` generic frame checks and CRC trimming:
  - `wmbusmeters/src/wmbus.cc`
- `wmbusmeters` Apator driver behavior:
  - `wmbusmeters/src/driver_apator162.cc`

## Troubleshooting

- `Freq not allowed`: region lock prevents `868.95 MHz` RX.
- many `Not plausible` statuses:
  - verify mode (`Left`/`Right`) against your meter transmit mode
  - confirm antenna/placement and RSSI stability
- frequent `CRC BAD`:
  - weak signal / collisions / wrong mode
  - check if frame count rises but `CRC OK` stays low
- no total volume shown for Apator:
  - telegram may be old `0xB6` style or unsupported register layout
