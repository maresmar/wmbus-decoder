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
- `OK`: toggle display mode (`Latest` <-> `History`)
- `Up`/`Down`: browse history only in `History` mode (`Latest` mode ignores browse keys)
- `Long Up`: toggle debug overlay
- `Back`: exit app

Mode commands are delivered to RX thread through a `FuriMessageQueue`, avoiding racy shared state.

## Runtime View Lines

- header right: `Latest` in live mode, or `H:<pos>/20` in history mode
- line 1: `DEC:<decoded> Rhi:<strong RSSI> OK:<crc_ok> BAD:<crc_bad>`
  - `Rhi` counts packets with RSSI at/above strong threshold (`-70 dBm`)
- line 2: `R:<packets_per_sec>/s RSSI:<last_live_rssi>`
- line 3: `PKT M:<T/C> R:<captured_rssi> S:<status>`
  - mode/rssi follow the currently shown packet in history; if no packet is shown, fallback is current sync mode + live RSSI

## Mode Selection For Apator162

Apator `AT-WMBUS-16-2` can be configured to transmit either `T1` or `C1`.
There is no universal default that always works in the field.

- try `T` first (`Left`)
- if counters stay at `0 decoded`, switch to `C` (`Right`)
- keep the mode where decoded/CRC counters start moving

Why both are needed:

- `T` mode uses 3-of-6 coding and needs software decode/framing
- `C` mode is direct-byte with whitening and can use CC1101 variable-length packet engine

## RX/Decode Architecture

### Radio

- CC1101 custom preset for WM-Bus receive at `868.95 MHz`
- mode-specific packet handling:
  - `T`: infinite-length RX, software framing
  - `C`: variable-length packet mode + whitening

### Packet processing

1. Capture path by mode:
- `T`: stream raw FIFO bytes, estimate expected raw length from decoded `L-field`, complete on timeout/full/expected-length.
- `C`: use packet engine payload reads, reconstruct frame as `[L][payload]`.
2. Decode path:
- `T`: 3-of-6 decode into byte stream.
- `C`: direct byte stream after reconstruction.
3. Plausibility gates:
- minimum header length
- valid `L-field`
- valid WM-Bus `C-field` (`0x44` or `0x46`)
- valid manufacturer code shape
4. Length + EN13757 CRC checks.
5. Update view model/history and confidence counters.

### Frame format handling

- Frame format (`A`/`B`) is selected by `L-field` length fit plus DLL CRC validation.
- Preferred order follows RX mode (`T`: try `A` then `B`, `C`: try `B` then `A`), with fallback when CRC is bad.
- Once selected, DLL CRC bytes are stripped before CI/TPL/vendor parsing and model updates.

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
- both modes show `0 decoded`:
  - verify meter is transmitting in your test window (some intervals are minutes apart)
  - confirm region/frequency access to `868.95 MHz`
  - reduce distance and improve antenna orientation
- frequent `CRC BAD`:
  - weak signal / collisions / wrong mode
  - check if frame count rises but `CRC OK` stays low
- no total volume shown for Apator:
  - telegram may be old `0xB6` style or unsupported register layout
