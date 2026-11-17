#!/usr/bin/env python3
"""Static Rev27 PCB-to-firmware interface consistency check (no KiCad CLI needed)."""
from pathlib import Path
import csv, re, sys

ROOT = Path(__file__).resolve().parents[2]
PCB = ROOT / "BCI_AFE_Processing_Controls_COMMERCIAL_STYLE.kicad_pcb"
AFE_SCH = ROOT / "01_Electrode_AFE.kicad_sch"
SW = ROOT / "software"

def balanced_block(source: str, start: int) -> str:
    depth = 0; quoted = False; escaped = False
    for i in range(start, len(source)):
        ch = source[i]
        if quoted:
            if escaped: escaped = False
            elif ch == "\\": escaped = True
            elif ch == '"': quoted = False
        else:
            if ch == '"': quoted = True
            elif ch == "(": depth += 1
            elif ch == ")":
                depth -= 1
                if depth == 0: return source[start:i+1]
    raise RuntimeError("Unbalanced KiCad S-expression block")

def main() -> int:
    pcb = PCB.read_text(encoding="utf-8")
    marker = '(footprint "Package_QFP:LQFP-144_20x20mm_P0.5mm"'
    pos = pcb.find(marker)
    if pos < 0: raise RuntimeError("U50 LQFP144 footprint not found")
    u50 = balanced_block(pcb, pos)
    if '(property "Reference" "U50"' not in u50:
        raise RuntimeError("Matched LQFP144 footprint is not U50")
    actual = {int(pin): net for pin, net in re.findall(
        r'\(pad "(\d+)"[^\n]*?\(net \d+ "([^"]+)"\)', u50)}
    expected = {
        1: "AFE_SPI_SCK_MCU", 3: "AFE_SPI_CS_MCU", 4: "AFE_SPI_MISO",
        5: "AFE_SPI_MOSI_MCU", 34: "AFE_DRDY", 49: "AFE_START",
        50: "AFE_RESET", 53: "AFE_PWDN",
    }
    for pin, net in expected.items():
        if actual.get(pin) != net:
            raise RuntimeError(f"U50 pad {pin}: expected {net}, found {actual.get(pin)}")

    pinmap = SW / "docs/REV27_AFE_MCU_PIN_MAP.csv"
    with pinmap.open(newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))
    if {int(r["U50_pad"]): r["PCB_net"] for r in rows} != expected:
        raise RuntimeError("REV27_AFE_MCU_PIN_MAP.csv does not match expected PCB nets")

    header = (SW / "firmware/platform/stm32h7/hb_board_pinmap_rev27.h").read_text()
    for macro in ["HB_ADS1299_SPI_HANDLE hspi4", "GPIO_PIN_4", "GPIO_PIN_0",
                  "GPIO_PIN_11", "GPIO_PIN_12", "GPIO_PIN_13"]:
        if macro not in header: raise RuntimeError(f"Board header missing {macro}")
    if "CLKSEL_GPIO" in header:
        raise RuntimeError("CLKSEL must not be treated as an MCU GPIO in this revision")

    sch = AFE_SCH.read_text(encoding="utf-8")
    j1_pos = sch.find('(property "Reference" "J1"')
    if j1_pos < 0: raise RuntimeError("J1 placed instance not found")
    # Scope the J1 placed-instance block, not any cached library definition.
    begin = sch.rfind("(symbol", 0, j1_pos)
    j1_block = balanced_block(sch, begin)
    if "(dnp yes)" not in j1_block:
        raise RuntimeError("J1 must remain DNP / SAFETY HOLD")

    print("Rev27 PCB -> firmware interface check: PASS")
    print("  U50 AFE nets verified against LQFP144 pads: 8/8")
    print("  Firmware pin-map CSV/header synchronization: PASS")
    print("  J1 DNP safety hold: PASS")
    return 0

if __name__ == "__main__":
    try: raise SystemExit(main())
    except Exception as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        raise SystemExit(1)
