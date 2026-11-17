# STM32H743 integration gate

`hb_stm32h7_port.c` is an STM32 HAL adapter for the planned Rev30/32 dual-ADS1299 pin assignments. The second ADC is not present on the current PCB; these mappings are prospective and must be checked against the next board netlist before flashing.

Before building the target:
1. Export the KiCad netlist from the exact Rev27 project.
2. Planned U1/U109 controls use shared SPI4 and START with separate CS, RESET, PWDN and DRDY GPIOs. See `hb_board_pinmap_rev30.h` and `software/docs/REV30_DUAL_ADS1299_MCU_PIN_MAP.csv`.
3. Confirm the Rev27 U50 footprint pad numbering and assembly orientation. Configure SPI4 mode 1, 8-bit, MSB-first, software chip select and conservative SCLK at or below 1 MHz. Configure PE2/PE5/PE6 to AF5; CS, START, RESET and PWDN as GPIO outputs; PA0 as a DRDY input.
4. The ADS1299 CLKSEL signal is strapped low in the schematic; it is not an MCU GPIO. The external 2.048 MHz clock oscillator X51 must be populated and verified with a scope/frequency counter before enabling the driver.
5. Implement `HB_BOARD_AFE_RAILS_GOOD()` from verified rail monitoring and `HB_BOARD_AFE_VCAP1_READY()` only from a measured or otherwise characterized startup bound proving VCAP1 reaches at least 1.1 V. No MCU VCAP1 monitor is shown in the schematic.
6. Implement `hb_board_micros()` using a monotonic, free-running 1 MHz timer. HAL_GetTick() is too coarse to measure 1 kSPS sample timing.
7. Add the transport packet sink only after the CubeMX USB device stack, ADuM3166 power/isolation topology, and endpoint speed have been validated.

The AFE GPIO/SPI mapping is present because it can be traced to U50 pad numbers in the Rev27 PCB and the STM32H743 LQFP144 pinout. It is not evidence that CubeMX clocks, GPIO initialization, board power sensing, or actual silicon operation have been validated.


`hb_board_hooks.c` supplies the common application hooks that connect `main.c` to the AFE HAL adapter and USB CDC queue. The CubeMX project must provide `hb_board_hw_init()`, `hb_board_fault()`, `hb_board_afe_rails_good()` and `hb_board_afe_vcap1_ready()`. VCAP1 is not connected to an MCU monitor in this schematic; implement readiness only using a characterized external measurement or a validated conservative timing bound. The optional `hb_board_flash_init()` initializes the W25Q128 driver but does not start logging.
