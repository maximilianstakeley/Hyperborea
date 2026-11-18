# Rev16 bench bring-up

Open Hardware/Rev16/00_Hyperborea.kicad_pro and Bench/Fixture/Fixture.kicad_pro as separate KiCad projects. Assemble the fixture only for simulator work. All baseboard signal pins and the service pins occupy one type B DIN 41612 receptacle. Use a full-width keyed plug and insulated enclosure. Populate only the assigned contacts in each mating cassette. Verify shell geometry and accessibility on the assembled enclosure.

Connect a floating 20 Hz generator to fixture J2. Set a 25% duty positive pulse at 10 mV peak-to-peak. Set the selected JP to 1–2 and the other fifteen to 2–3. Remove a jumper for a disconnected positive lead. Check the generator output at the fixture under load. Send START, allow the 60 s baseline qualification, then record at least two seconds of whole v2 packets per selected channel. Run this in the repository root for each capture:

    python3 Bench/verify_capture.py channel01.hbci 1

The checks cover CRC, sample rate, sequence, both status words, polarity, amplitude and cross-channel response. Host capture files must start at a packet boundary. For common-mode tests lift both returns from the source and use an external isolated common-mode injection fixture referenced to the bias contact. For electrode offsets use an isolated series offset source. Measure bias relay opening, powered-down injection and all-contact leakage separately.

## Signal budget

Run python3 Bench/input_budget.py. The linear AC model includes the 540 kΩ protection chain, 100 nF coupling, 10 MΩ return, 100 kΩ final series path, 10 pF shunts and 330 pF differential capacitor. It sweeps independent component extrema per leg, 0–10 kΩ electrode resistance and 0–100 pF cable shunt to local AC ground. It keeps the common differential capacitor nominal. At 50/60 Hz the smallest modeled CMRR is about 50 dB. A 10 mV common-mode disturbance can therefore produce about 29 µV differential output, large enough to obscure EEG. Match leads and electrode impedances, screen differential pairs, and measure both balanced and imbalanced cases. A 1 µV input-referred mains allocation at 10 mV requires 80 dB. Rev16 does not close that target at the modeled extremes.

At 40 °C the estimated differential series-resistance noise is 1.50 µV RMS over 100 Hz. For a 2 µV RMS electronics target this leaves 1.32 µV RMS in quadrature for the ADC, clamp current noise, bias returns and other sources. Measure shorted inputs with representative source impedance and actual digital filtering. For gain 12 the ADC range is ±375 mV. Allocate 150 mV differential static leakage error, equivalent to 7.5 nA net leakage per leg with opposite signs across 10 MΩ. The 5 nA diode figure at 25 °C already consumes 100 mV differential. Verify the remaining 2.5 nA/leg for ADC bias, capacitor and PCB leakage and hot clamp leakage. An out-of-budget hot result requires lower-leakage components or redesign. Neither this signal-offset budget nor resistor current estimates establish patient leakage.

## Firmware and service

Build the STM32 target with HB_BOARD_REV16 and the include paths for Firmware/Platform and the public hb headers. The older CubeH7 board hooks require generated HAL startup, GPIO, SPI4, USB FS CDC and a free-running 1 MHz timebase. The checked-in repository does not contain a complete generated target. Implement the documented receive and transmit-complete callbacks on the FS interface. SPI4 kernel clock must divide to 3.2–4 MHz. Two 27-byte transfers take 108–135 µs plus 6 µs chip-select hold. At 4 kSPS the theoretical remaining time is 109–136 µs. Measure from DRDY to final chip-select release under simultaneous USB traffic. Required USB payload is 288 kB/s including 72-byte framing.

START enables bias and qualifies 240,000 consecutive samples within half-scale and within 10 mV of the initial sample for each channel before packet output. STOP drops bias and conversion requests. In service mode inspect hb_last_reset_flags and hb_last_afe_faults over SWD before sending ACK over USB. Fault bits 0 through 4 represent analog undervoltage, analog overvoltage, input eFuse, external watchdog and AFE digital window. Flags accumulate while MCU_3V3 is present. Complete supply removal loses latch history. A cold power-up can set startup flags and cannot identify an earlier shutdown.

TI ADS1299 datasheet sections 7.6, 9.5, 9.6 and 11.1 specify serial timing, register fields and startup: https://www.ti.com/lit/ds/symlink/ads1299.pdf
