# Rev28 implementation and verification report

**Date:** 2026-11-04  
**Hardware baseline:** Rev27 engineering correction package  
**Software revision:** Rev28  
**Status:** Native software tests pass; embedded target and physical board are not validated.

## Scope delivered

### Portable C11 firmware core
- ADS1299 power-up sequencing, reset, device identification, register configuration and readback.
- 1 kSPS / gain-24 / normal-input-mux provisional profile using the schematic's 2.048 MHz external clock.
- Explicit START GPIO conversion control, RDATAC sample streaming, DRDY timeout and signed 24-bit channel decoding.
- Acquisition state machine, latched fail-safe handling and a bounded sample-to-packet sink.
- HBCI v1 packet encoder and bounded SPSC USB transmit queue.
- HBCI v2 dual-ADC 16-channel packet encoder and paired acquisition state machine with microsecond timestamps.
- W25Q128 low-level driver: JEDEC ID check, WIP/WEL polling, read, page-boundary-safe program and 4 KiB sector erase.

### C++17 host software
- Incremental HBCI parser with framing recovery and CRC verification.
- Backward-compatible v1/v2 C++ parser and dual-stream timestamp-rate measurement in `hb_decode`.
- `hb_capture`: POSIX USB CDC serial capture to raw `.hbci` files.
- `hb_decode`: binary capture to CSV with sequence-gap and corrupt-frame reporting.

### STM32H7 integration source
- Rev27 AFE GPIO/SPI4 pin-map header and interface audit script.
- STM32H7 HAL AFE adapter and QSPI flash adapter.
- USB CDC HS bridge glue, application loop and board-hook integration layer.
- Integration instructions for CubeMX-generated startup, USB callbacks, rail-good and VCAP1 readiness functions.

## Two largest functional gaps addressed

1. **No working device-side acquisition software.** The portable C driver, acquisition state machine, fail-closed fault handling and STM32H7 integration layer were added.
2. **No device-to-host data contract or host acquisition tools.** HBCI v1, C encoder, C++ parser, live capture utility and CSV decoder were added and tested together.

## Additional critical corrections

- ADS1299 command-mode sequencing was corrected. The device enters RDATAC after register configuration; conversion start/stop is controlled by the dedicated START GPIO. The firmware no longer sends START/STOP opcodes while RDATAC is active.
- W25Q128 support was added because the schematic includes this memory but the previous software tree had no flash driver.

## Native verification performed

Build command:

```sh
cmake -S software -B software/build -DCMAKE_BUILD_TYPE=Debug
cmake --build software/build --parallel
ctest --test-dir software/build --output-on-failure
python3 software/tools/verify_rev27_interfaces.py
```

Results:

| Check | Result |
|---|---|
| Strict-warning C11/C++17 build | PASS |
| ADS1299 simulated-device driver test | PASS |
| C encoder ↔ C++ decoder interoperability | PASS |
| Packet queue ownership/saturation test | PASS |
| W25Q128 mock test | PASS |
| Rev27 PCB AFE pin-map consistency | PASS, 8/8 signals |
| J1 DNP safety-hold check | PASS |
| HBCI binary-to-CSV smoke test | PASS, one valid sample decoded |
| AddressSanitizer + UndefinedBehaviorSanitizer test run | PASS, 4/4 tests |

## Explicitly not verified

- No STM32H743 CubeMX `.ioc`, linker/startup project or generated HAL middleware was supplied as part of the Rev27 source package; therefore the embedded platform files were not compiled against the exact STM32CubeH7 version.
- No physical ADS1299 SPI, DRDY, START, RESET, PWDN, external-clock or VCAP1 waveform has been measured.
- The schematic does not route VCAP1 to an MCU monitor. The board project must implement `hb_board_afe_vcap1_ready()` using a characterized method; do not simply return true without engineering evidence.
- The AFE rail-good implementation is board-specific and must be supplied.
- USB CDC descriptors/callback integration, enumeration, isolation behavior and sustained throughput have not been hardware-tested.
- The QSPI adapter uses standard single-line W25Q commands through the STM32 QUADSPI peripheral. Quad-I/O mode, QSPI pin mapping and timing have not been validated.
- A durable flash logging task/format, power-loss recovery and wear-leveling are not included. Flash erase/program operations must not run in the real-time acquisition loop.
- No native KiCad ERC/DRC, PCB routing, hardware-in-loop, electrical safety, EMC, clinical or regulatory verification is claimed.

## Safety and scope

The hardware remains an eight-channel research prototype, not a completed 64-channel system. J1 remains DNP / SAFETY HOLD. Do not connect the board to a person or animal. This software is not medical-device firmware and is not a release for fabrication or biological use.
