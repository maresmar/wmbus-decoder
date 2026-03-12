# WM-Bus Decoder (Flipper Zero)

Developer-focused notes for `applications_user/wmbus_decoder`.

## Copilot Instructions

Use this as startup context for future coding chats in this app.

- Scope:
  - Work only inside `applications_user/wmbus_decoder/` unless explicitly requested.
  - Keep live RX mode changes inside the config scene only.
- Target meter:
  - Primary target is Apator `AT-WMBUS-16-2` (`apator162`).
  - Old Apator style with `CI=0xB6` is out of scope unless asked.
- RX architecture constraints:
  - RX control is queue-driven (`FuriMessageQueue`) using runtime config snapshots.
  - Do not reintroduce shared volatile mode/running flags across threads.
  - Keep plausibility gate for WM-Bus `C-field` valid values (`0x44`, `0x46`).
  - Keep frame length trimming before CRC/model updates.
- Data model/UI:
  - The app now has a live RX scene, a config scene, and a packet detail view.
  - History entries store their RX tick so the UI can show packet age while browsing.
  - Normal view should show parser-provided primary fields when available.
  - Settings use status thresholds for memory and CSV retention.
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
- configurable packet retention/logging by decode status
- generic parser output with targeted payload extraction for Apator `AT-WMBUS-16-2`
- optional AES mode-5 decryption using keys from `keys.txt`

## Folder Layout

```text
applications_user/wmbus_decoder/
  app/         app entry point and runtime shell
  core/        shared types and compile-time config
  protocol/    packet pipeline, capture, frame, parser, crypto
  storage/     settings, paths, keyring, CSV logging
  test/        selftest harness
  ui/          scenes, views, and legacy archived UI
```

## Build and Install

From firmware root:

```bash
./fbt fap_wmbus_decoder
```

Compile-time selftests are currently enabled in `application.fam` with
`WMBUS_SELFTESTS=1`. The app runs the selftest harness before GUI/radio init and prints results through
`FURI_LOG_I` / `FURI_LOG_W`. The report is also written to
`/ext/apps_data/wmbus_decoder/selftest.txt`.

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

Regression vector sources:

- clear-text Apator corpus copied from the vendored `wmbusmeters/src/driver_apator162.cc`
- matching JSON expectations cross-checked with `wmbusmeters/simulations/simulation_apas.txt`
- encrypted zero-key Apator sample cross-checked with `wmbusmeters/simulations/serial_aes.msg`
- two additional encrypted Apator gold telegrams are field samples provided on March 12, 2026 with expected totals `345.654 m3` and `200.257 m3`

Important constraints:

- no software PN9 whitening/dewhitening is implemented
- Mode `C` assumes bytes already match the parser-visible post-radio form
- Mode `T` selftests validate only the software 3-of-6/offset/parser path

## Runtime Controls

- `OK`: toggle display mode (`Latest` <-> `History`)
- `Up`/`Down`: browse history only in `History` mode (`Latest` mode ignores browse keys)
- `Long OK`: open config screen
- `Long Down`: open packet detail view for the selected history entry
- `Back`: exit app

Mode and logging changes are delivered to RX through a queued runtime-config snapshot, avoiding racy shared state.

## Config Screen

The config scene currently supports:

- RX mode: `T` / `C`
- CSV logging: `None` / `Basic` / `Full`
- memory threshold: `Store if >=`
- CSV threshold: `Log if >=`
- keyring status display and key entry through the UI

CSV files are written to:

- `/ext/apps_data/wmbus_decoder/packets_basic.csv`
- `/ext/apps_data/wmbus_decoder/packets_full.csv`

Settings are persisted in:

- `/ext/apps_data/wmbus_decoder/settings.txt`

## Key File

Optional decryption keys are loaded from:

- `/ext/apps_data/wmbus_decoder/keys.txt`

Line format:

```text
00112233445566778899AABBCCDDEEFF
```

- one 16-byte AES key per line as 32 hex characters
- lines starting with `#` are ignored

Keys are tried in file order. For short-TPL mode-5 telegrams the shared packet path also tries the legacy all-zero key, but a decrypted candidate is only accepted when it has visible `2F2F` check bytes or a registered device parser validates the payload.

## Runtime View Lines

- header right: `Latest` in live mode, or `H:<pos>/<count>` in history mode
- line 1: `DEC:<decoded> Rhi:<strong RSSI> OK:<crc_ok> BAD:<crc_bad>`
  - `Rhi` counts packets with RSSI at/above strong threshold (`-70 dBm`)
- line 2: `R:<packets_per_sec>/s RSSI:<last_live_rssi>`
- line 3: `Last <status>` or `Pkt <status>`, with `A:<age>` right-aligned for the currently shown packet
- line 4: left `MF:<manufacturer> DT:<device_type>`, right `ID:<meter_id>`
- line 5: `M:<rx_mode> R:<packet_rssi>` followed by crypto status and either `CI:<ci>` or parsed total

## Mode Selection For Apator162

Apator `AT-WMBUS-16-2` can be configured to transmit either `T1` or `C1`.
There is no universal default that always works in the field.

- try `T` first in the config screen
- if counters stay at `0 decoded`, switch to `C` in the config screen
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
5. Build a generic packet record, run shared decrypt candidate selection, then let registered device parsers claim validated payloads before routing the record through:
- memory history sink filtered by status threshold
- CSV sink filtered by status threshold and CSV mode

### Frame format handling

- Frame format (`A`/`B`) is selected by `L-field` length fit plus DLL CRC validation.
- Preferred order is currently `A` then `B`, with fallback when CRC is bad.
- Once selected, DLL CRC bytes are stripped before CI/TPL/vendor parsing and model updates.

## Apator `AT-WMBUS-16-2` Parsing

A targeted parser extracts total volume from proprietary register payloads.

Implemented behavior:

- requires `CI == 0x7A`
- requires Apator identity (`APA` or legacy manufacturer `0x8614`, version `0x05`, device type `0x07`) plus a validated proprietary payload layout
- for short-TPL security mode `5`, tries keys from `keys.txt` in file order
- after configured keys, the shared packet path also tries the legacy fixed all-zero 16-byte key
- mode-5 decrypt candidates are produced in shared code; a candidate is accepted when it has visible `2F2F` check bytes or the Apator parser validates the payload
- for other encrypted short-TPL modes, requires visible decrypted `2F2F` check bytes before parsing
- starts after short TPL header
- skips leading `0x2F` fillers
- when vendor header byte `0x0F` is present, skips the 8-byte Apator fixed block
- otherwise accepts payloads that begin directly with a known Apator register
- iterates vendor registers using `apator162` register-size table
- validates register boundaries until end marker / padding before trusting the parsed total
- extracts register `0x10` or `0xA1` as little-endian `uint32`
- interprets raw value as `m3 * 1000`

Displayed on normal UI when available as parser-provided primary fields, for example:

- `3.843 m3`
- `Key:#2`

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
