# W25Q128 QSPI integration

The current transport uses standard single-line SPI-compatible flash opcodes through the STM32 QUADSPI controller; it does not enable the flash quad-I/O mode. The portable `hb/w25q128.h` driver implements JEDEC identification, status polling,
write-enable verification, reads, page-boundary-safe page programming, and 4 KiB
sector erase. The STM32H7 HAL adapter is `hb_w25q128_qspi.c`.

Before target build:

1. Generate the CubeMX QSPI peripheral configuration for the exact U50 pin map.
2. Verify the W25Q128JV supply, IO voltage, clock mode, dummy cycles, and maximum
   SCLK against the actual flash datasheet and board routing.
3. Confirm `hqspi` is initialized before calling `hb_w25q128_init()`.
4. Call `hb_w25q128_init()` and require the exact JEDEC ID `EF 40 18`.
5. Keep erase/program operations out of the 1 kSPS acquisition polling path.
   Run flash work in a lower-priority storage task with its own bounded queue.
6. Define a persistent log format, power-loss recovery, wear-leveling policy,
   partition map, and erase scheduling before enabling automatic recording.

The low-level driver does not automatically erase user data, does not implement
wear-leveling, and is not yet wired into the acquisition task. This avoids a
blocking erase or flash-busy wait from stalling real-time sample acquisition.
