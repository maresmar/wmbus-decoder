# Receive Pipeline

The receive pipeline has a strict ownership boundary: the radio qualifies the
hardware sync, capture validates mode-specific FIFO framing, and packet decoding
produces and validates the Link Layer frame.

The CC1101 runs in infinite-length mode and its 64-byte RX FIFO is drained while
reception is active. `RXBYTES` can report 65 because it includes the radio's
one-byte prefetch buffer; that is a valid count, while a single FIFO read remains
limited to 64 bytes. `RXBYTES` is read repeatedly until two consecutive values
agree, as required by the CC1101 silicon errata. Until the complete calculated
frame is resident, every SPI read retains one guard byte in RX FIFO; only the
final read may empty it. Capture recovers the L-field, calculates the exact FIFO
length, and never consumes bytes beyond that boundary.

Hardware SFD assertion starts capture ownership even if no complete byte has yet
reached RX FIFO. This matters in infinite-length mode: a false sync with zero
FIFO bytes is still ended by the mode's incomplete-frame timeout and RX is
flushed and restarted. A low-rate `MARCSTATE` watchdog also restarts RX if the
radio leaves its receive states, including an RX FIFO overflow state.

The FIFO/PHY boundary produces one representation for both modes:
`WmBusPhyFrame::data` always starts at the Link Layer L-field and `format` is an
authoritative Frame A or Frame B value. Packet decoding never guesses a format.

## C mode

- CC1101 performs an exact 16/16-bit match on `54 3D`, with carrier-sense
  qualification (`MDMCFG2 = 0x06`). FIFO reception begins immediately afterward.
- Capture requires the first FIFO bytes to be a valid C-mode sync remainder:
  `54 CD` for Frame A or `54 3D` for Frame B. Any other remainder is rejected.
- Only after validation is the two-byte remainder removed. Packet decoding then
  receives the frame beginning at its L-field (`4E` in the real Frame A captures).

Thus the complete C-mode sync is validated deterministically:

```text
             hardware   software   complete
Frame A      54 3D      54 CD      54 3D 54 CD
Frame B      54 3D      54 3D      54 3D 54 3D
```

## T mode

- CC1101 matches `54 3D` with 15/16-bit plus carrier-sense qualification
  (`MDMCFG2 = 0x05`).
- A valid hardware sync establishes both bit and byte synchronization, so the
  first 3-of-6 symbol starts at FIFO bit zero. No offset search is permitted.
- FIFO capture starts at bit zero, validates every 3-of-6 symbol and converts the
  exact encoded length to a Frame A `WmBusPhyFrame`. Packet decoding receives the
  same Link Layer wire representation as it does for C mode.

Raw FIFO state remains confined to the capture layer. It is never passed to
packet parsing as though it were a Link Layer frame.

## Processing boundary

The radio thread ends at a completed `WmBusPhyFrame`. It submits the frame to a
bounded processing queue and immediately resumes FIFO service. Link-layer
processing, decryption, UI history, CSV formatting and SD-card synchronization
run on a lower-priority processing worker; storage latency therefore cannot
block RX FIFO draining. A full queue drops the new frame explicitly and reports
the cumulative drop count rather than blocking the high-priority radio thread.
