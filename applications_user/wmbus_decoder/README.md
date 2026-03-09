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
  - If no total is found but short TPL is present, show the short-TPL security mode and a compact encryption hint.
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

Compile-time selftests are currently enabled in `application.fam` with
`WMBUS_SELFTESTS=1`. Rebuild and launch the app once to run the in-app selftest
harness. Set that define back to `0` if you want normal startup without the
selftest pass. The selftests run before GUI/radio init and print their results
through `FURI_LOG_I` / `FURI_LOG_W`.
They also write a plain-text report to `/ext/apps_data/wmbus_decoder/selftest.txt`,
which makes it possible to verify a test run from the device without flashing a
`unit_tests` firmware image.

Output artifact:

- `build/f7-firmware-D/.extapps/wmbus_decoder.fap`

Deploy as you normally deploy external apps in your Flipper firmware workflow.

## In-App Selftests

When `WMBUS_SELFTESTS=1`, `wmbus_decoder_app()` calls `wmbus_run_selftests()`
once at startup.

This is now the only maintained test path for `wmbus_decoder`.

Covered scenarios:

- Mode `C` vectors using already-decoded telegram bytes
- Mode `T` synthetic 3-of-6 vectors with offset search across `0..7`
- field parsing, length computation, CRC pass/fail behavior
- negative cases for bad CRC, bad plausibility, and bad 3-of-6 symbols
- low-level 3-of-6 decode success/failure edges
- capture helpers for C-frame reconstruction, expected-length estimation, and state reset
- format-A and format-B normalization checks
- public Apator corpus parsing and old-style `CI=0xB6` rejection

Important constraints:

- no software PN9 whitening/dewhitening is implemented
- Mode `C` assumes bytes already match the parser-visible post-radio form
- Mode `T` selftests validate only the software 3-of-6/offset/parser path

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
  - baseline matches the TI Radio Link B receive preset, with mode-specific packet handling patched at runtime
- mode-specific packet handling:
  - `T`: infinite-length RX, software framing
  - `C`: infinite-length RX with hardware dewhitening; software strips an optional leading signaling byte before WM-Bus parsing

### Packet processing

1. Capture path by mode:
- `T`: stream raw FIFO bytes, estimate expected raw length from decoded `L-field`, complete on timeout/full/expected-length.
- `C`: stream dewhitened FIFO bytes, tolerate an optional leading `0x54` C-mode signaling byte, then parse the remaining bytes as a normal WM-Bus frame.
2. Decode path:
- `T`: 3-of-6 decode into byte stream.
- `C`: direct byte stream after signaling-byte stripping.
3. Plausibility gates:
- minimum header length
- valid `L-field`
- valid WM-Bus `C-field` (`0x44` or `0x46`)
- valid manufacturer code shape
4. Length + EN13757 CRC checks.
5. Update view model/history and confidence counters.

### Frame format handling

- Frame format (`A`/`B`) is selected by `L-field` length fit plus DLL CRC validation.
- Preferred order is currently `A` then `B`, with fallback when CRC is bad.
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

When total volume is not found but short TPL is present, the fallback line shows:

- `CI:<ci> S:<mode> E:<flag> R:<rssi>`
  - `S` is the raw short-TPL security mode from `CFG`
  - `E:Y` means a known OMS encrypted mode (`5/7/8/9/10`)
  - `E:?` means a nonzero but non-classified security mode
  - `E:N` means no short-TPL security mode bits are set

CRC-valid Apator frames that still do not expose `total_m3` are also dumped to the log as
chunked normalized-frame hex (`APA norm[...]`) to make it easier to inspect unsupported or
secured payloads.

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
