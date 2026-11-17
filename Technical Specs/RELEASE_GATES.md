# Rev28 software release gates

## Completed in source and native tests
- C11 AFE driver compiles with strict warnings.
- ADS1299 simulated SPI test covers reset/configuration, ID check, register readback, sample decoding and acquisition fault stop.
- C encoder and C++17 stream parser share the same packet layout.
- C++ host capture and decoder compile on POSIX; host parser test covers split chunks, noise resynchronization and CRC rejection.
- Packet queue test covers saturation, retry after a rejected USB submission, in-flight buffer retention and completion.
- W25Q128 mock test covers JEDEC identity, cross-page programming, readback, sector erase and range rejection.
- ADS1299 conversion control uses the dedicated START pin, avoiding command opcodes while RDATAC is active.

## Required before target flashing
- Recheck STM32H743ZIT6 physical pin map and complete the CubeMX `.ioc` (clock tree, GPIO, SPI4, QSPI and USB).
- Confirm SPI mode/rate and the AFE external clock with oscilloscope/logic analyzer.
- Confirm every ADS1299 configured register and analog gain/reference choice against exact board population and TI datasheet.
- Complete the STM32Cube USB CDC HS callback integration; validate link-speed negotiation, host enumeration, sustained throughput, dropped-frame handling and reset recovery.
- Validate QSPI pin mux, clock mode, flash timing and the HAL QSPI adapter on the populated board.
- Implement a background storage task, durable log format, wear-leveling and power-loss recovery before enabling local logging.

## Required before any electrode interface use
- Close electrical safety, leakage/fault current, isolation, ESD, EMC, input protection, connector, and regulatory verification.
- J1 must remain DNP until those gates are formally closed. No person or animal connection is allowed under this package.
