# HBCI sample packet version 1

All multi-byte header and CRC values are little-endian. ADC status and channel sample values retain the ADS1299 big-endian, 24-bit wire order within their respective three-byte fields.

| Byte offset | Length | Field | Encoding |
|---:|---:|---|---|
| 0 | 4 | Magic | ASCII `HBCI` |
| 4 | 1 | Version | `1` |
| 5 | 1 | Packet type | `1` = sample |
| 6 | 2 | Total packet length | 45, little-endian |
| 8 | 4 | Sequence | Unsigned 32-bit, little-endian |
| 12 | 4 | Device timestamp | Milliseconds, unsigned 32-bit, little-endian |
| 16 | 3 | ADS1299 status | Big-endian unsigned 24-bit |
| 19 | 24 | Channels 1–8 | Eight big-endian signed two's-complement 24-bit values |
| 43 | 2 | CRC | CRC-16/CCITT, init `0xFFFF`, polynomial `0x1021`, no reflection/xorout; little-endian |

Total length is 45 bytes. Sequence increments only after the packet sink accepts the packet. Timestamp wraps naturally at the 32-bit millisecond rollover. Host tools must use unsigned modular sequence arithmetic if they need to handle rollover.

## HBCI sample packet version 2 (dual ADS1299)

Version 2 is used for paired 16-channel samples. The header layout and CRC parameters remain the same; version is `2`, packet type is `2`, and total length is 72 bytes. The timestamp field is a monotonic microsecond tick (32-bit, wrap-safe) rather than milliseconds. Payload fields are two big-endian unsigned 24-bit ADS1299 status words followed by sixteen big-endian signed 24-bit channel values, ordered AFE1 channels 1–8 then AFE2 channels 9–16. The final CRC covers the 16-byte header plus 54-byte payload. The C encoder, C++ decoder, mixed-version stream parser and 1 kSPS timestamp monitor are covered by native mock/software tests; these do not constitute a physical ADC-rate measurement.
