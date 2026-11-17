#!/usr/bin/env python3
"""Static consistency check for Rev30 dual-ADS1299 MCU GPIO assignments."""
from pathlib import Path
import re
ROOT = Path(__file__).resolve().parents[2]
PCB = ROOT / "BCI_AFE_Processing_Controls_COMMERCIAL_STYLE.kicad_pcb"
SCH = ROOT / "03_Digital_Processing.kicad_sch"
FW = ROOT / "software/firmware/platform/stm32h7/hb_board_pinmap_rev30.h"
text = PCB.read_text()
expected = {
    48: "AFE2_SPI_CS_MCU",
    134: "AFE2_RESET_MCU",
    136: "AFE2_PWDN_MCU",
    137: "AFE2_DRDY_MCU",
}
# Find the U50 footprint and inspect only its pad records.
blocks = re.findall(r"\(footprint .*?(?=\n\t\(footprint |\n\(gr_|\Z)", text, re.S)
u50 = next((b for b in blocks if '(property "Reference" "U50"' in b), None)
assert u50 is not None, "U50 footprint missing"
for pad, net in expected.items():
    match = re.search(rf'\(pad "{pad}"[^\n]*?\(net \d+ "([^"]+)"\)', u50)
    assert match and match.group(1) == net, f"U50 pad {pad}: expected {net}, got {match.group(1) if match else None}"
sch = SCH.read_text()
fw = FW.read_text()
for net in expected.values():
    assert f'global_label "{net}"' in sch, f"Missing schematic label {net}"
    assert net in fw, f"Missing firmware pin map net {net}"
assert "GPIO_PIN_2" in fw and "GPIO_PIN_4" in fw and "GPIO_PIN_6" in fw and "GPIO_PIN_7" in fw
print("Rev30 U50 pad/net/schematic/firmware static consistency: PASS")
