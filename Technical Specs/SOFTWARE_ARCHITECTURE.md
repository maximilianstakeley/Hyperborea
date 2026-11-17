# Rev28 software architecture

## Hardware facts adopted from Rev27
- One placed ADS1299IPAG at U1: eight simultaneously sampled channels.
- STM32H743ZIT6 is the digital controller.
- AFE clock net is named `AFE_CLK_2M048` (2.048 MHz).
- Logical AFE nets: `AFE_SPI_CS`, `AFE_SPI_SCK`, `AFE_SPI_MISO`, `AFE_SPI_MOSI`, `AFE_DRDY`, `AFE_RESET`, `AFE_PWDN`, `AFE_START`.
- QSPI flash: W25Q128JVSIQ; USB topology includes USB3320 and ADuM3166.
- J1 stays DNP / SAFETY HOLD.

## Two largest functional problems and Rev28 fixes

### 1. There was no executable AFE/controller firmware
**Fix:** added portable C11 ADS1299 startup, ID check, explicit register writes/readback, TI power-up timing (130 ms minimum POR hold at the 2.048 MHz clock), a VCAP1-ready gate, external-clock selection, reference-settle hold, command-mode handling, DRDY timeout, 27-byte conversion-frame decode, explicit start/stop, and a fail-closed acquisition loop. The profile defaults to 1 kSPS (CONFIG1=0x94 with a 2.048 MHz AFE clock), gain 24, normal channel mux and internal reference buffer enabled. BIAS and lead-off circuits are not enabled in software because the electrode and bias configuration are not released. The driver enters RDATAC only after register setup and controls conversion with the dedicated START GPIO (high=start, low=stop); it does not send START/STOP opcodes while RDATAC is active.

### 2. There was no single end-to-end sample contract or host parser
**Fix:** defined a single 45-byte HBCI v1 sample packet. The C firmware encoder and C++17 parser share the same byte-level protocol; CRC-16/CCITT, explicit sequence/timestamp, packet length/type/version, 24-bit status and eight signed 24-bit samples are checked. The host tool decodes binary captures into CSV and reports sequence gaps and rejected frames. Native tests test the C-to-C++ boundary and parser recovery.

## State model
`power/reset -> rails checked -> POR/VCAP1 gate -> SDATAC -> device ID checked -> registers configured -> all registers read back -> RDATAC -> initialized -> START GPIO high -> DRDY/data frame -> packet sink -> START GPIO low`. Any acquisition timeout, SPI failure or sink failure stops streaming and latches a fault. There is no automatic recovery loop that could hide corrupted data.

## Transport and storage boundaries
The packet sink uses a bounded 15-frame SPSC queue. Queue entries are retained until the USB transmit-complete callback releases their buffer, so DMA never reads a reused stack buffer. The C++ host utility `hb_capture` sends `START`/`STOP` over CDC and records raw sample bytes; `hb_decode` converts captures to CSV. `firmware/app/main.c` services explicit start/stop requests and does not auto-start on boot. The USB3320/ADuM3166 schematic does not itself provide a host protocol or firmware stack. USB CDC/WinUSB descriptors, endpoint behavior, isolation throughput and reset recovery require integration with the actual USB middleware and must be verified. A portable W25Q128 driver and STM32H7 HAL QSPI transport adapter are now included. They support JEDEC identification, status/WEL polling, reads, page-boundary-safe programming and 4 KiB sector erase. A durable acquisition log, power-loss recovery, wear-leveling, verified QSPI pin mapping and background storage task are not yet implemented; flash operations must not run in the 1 kSPS acquisition loop.

## Additional functional defects corrected during review
- The initial driver design attempted to issue START/STOP SPI opcodes while RDATAC was enabled. TI specifies that commands other than SDATAC are not accepted in RDATAC mode. The final driver now uses the dedicated START GPIO for conversion control and keeps RDATAC enabled for sample reads.
- The W25Q128 previously had no driver. The new low-level driver adds JEDEC ID validation, WEL/WIP handling, page-boundary-safe writes, reads, and sector erase. It deliberately does not erase automatically or block the acquisition loop.

## What has not been implemented or proven
- 64-channel acquisition (Rev27 has one eight-channel ADS1299).
- Future 3,072-channel ASIC/module design.
- Complete CubeMX project with generated clocks, SPI4/QSPI/USB configuration and board-specific rail/VCAP1 readiness functions.
- USB device middleware / host transport driver.
- Durable W25Q128 acquisition log, power-loss recovery, wear-leveling, and verified CubeMX QSPI pin configuration.
- Physiological filters, calibration, gain optimization, lead-off or BIAS operation.
- Hardware-in-loop testing, electrical noise/linearity tests, impedance testing, safety and regulatory validation.
