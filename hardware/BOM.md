# Bill of Materials – STM32F103C8T6 Low-Voltage Prototype | 低压原型 BOM

> **Scope**: Components needed for the isolated, current-limited low-voltage
> validation prototype described in `docs/VALIDATION_CHECKLIST.md`.
> This BOM does **not** cover a high-voltage or mains-connected build.
>
> All values must be confirmed against your actual schematic before ordering.
> Substitutions require re-validation (see Phase 1 of the checklist).

---

## Microcontroller and Programming

| Ref | Description | Value / Part | Qty | Notes |
|---|---|---|---|---|
| U1 | MCU | STM32F103C8T6 (Blue Pill) | 1 | Confirm Flash ≥ 64 KB and 8 MHz crystal |
| J1 | SWD debug header | 4-pin 2.54 mm | 1 | ST-Link V2 or clone |

---

## Power Supply

| Ref | Description | Value / Part | Qty | Notes |
|---|---|---|---|---|
| U2 | 3.3 V LDO | AMS1117-3.3 or HT7333 | 1 | Only required if not using on-board Blue Pill regulator |
| C1, C2 | LDO input/output caps | 10 µF / 16 V electrolytic | 2 | |
| F1 | Bench-side fuse | 500 mA slow-blow (mini blade or inline) | 1 | Mandatory before power input |

---

## Current Sensing

| Ref | Description | Value / Part | Qty | Notes |
|---|---|---|---|---|
| R_shunt | Shunt resistor | 0.1 Ω / 1 W / 1% | 1 | Adjust gain for ±5 A range |
| U3 | Current-sense amplifier | INA199A2DCKR (×50) or INA180A2IDBVR | 1 | Output biased to 1.65 V for bipolar sensing |
| R1, R2 | Bias divider for bipolar mid-rail | 10 kΩ each | 2 | 3.3 V → 1.65 V virtual ground |
| C3 | Amp output filter cap | 100 nF / 10 V ceramic | 1 | Connect to PA0 ADC input |

---

## Bus Voltage Sensing

| Ref | Description | Value / Part | Qty | Notes |
|---|---|---|---|---|
| R3 | Upper divider resistor | 100 kΩ / 0.1% | 1 | Scale 12 V → 1.2 V (for 12 V max bus) |
| R4 | Lower divider resistor | 12.4 kΩ / 0.1% | 1 | Calculate to match full-scale at max bus V |
| C4 | Anti-alias cap | 10 nF ceramic | 1 | RC with R4 → PA1 |

---

## Temperature Sensing

| Ref | Description | Value / Part | Qty | Notes |
|---|---|---|---|---|
| TH1 | NTC thermistor | 10 kΩ B3950 | 1 | Mount on power stage or MOSFET heatsink |
| R5 | Pull-up resistor | 10 kΩ / 1% | 1 | 3.3 V → NTC → PA2 → GND |
| C5 | Filter cap | 100 nF ceramic | 1 | Parallel with lower NTC leg |

---

## RS-485 Communication

| Ref | Description | Value / Part | Qty | Notes |
|---|---|---|---|---|
| U4 | RS-485 transceiver | MAX485ESA+ or SP485CN | 1 | Half-duplex, 5 V compatible |
| J2 | RS-485 terminal | 3-pin 3.5 mm screw (A/B/GND) | 1 | |
| R6 | Line termination (optional) | 120 Ω / 0.25 W | 1 | Only at cable ends; omit for short cables |
| R7, R8 | Bias resistors (optional) | 560 Ω each | 2 | Pull A high, B low when bus idle |

---

## Gate Driver and Power Switch (Low-voltage MOSFET stage)

| Ref | Description | Value / Part | Qty | Notes |
|---|---|---|---|---|
| U5 | Gate driver | IR2104 (half-bridge) | 1 | Single-supply bootstrap driver, 5–20 V |
| Q1, Q2 | N-channel MOSFET | IRLZ44N (55 V / 47 A, logic-level) | 2 | Or AO3400 in SOT-23 for SMD layout |
| D1 | Bootstrap diode | 1N4148 or BAT54 | 1 | Required for IR2104 bootstrap circuit |
| C_boot | Bootstrap capacitor | 100 nF / 25 V ceramic | 1 | Pin BS–VS of IR2104 |
| R9, R10 | Gate resistors | 10 Ω / 0.25 W | 2 | One per MOSFET |
| R11, R12 | Gate pull-down | 10 kΩ | 2 | Prevent floating gate |

---

## Inductor and Filter

| Ref | Description | Value / Part | Qty | Notes |
|---|---|---|---|---|
| L1 | Power inductor | 100 µH / 2 A (e.g. SRR1260-101Y) | 1 | Sized for 20 kHz, ≤0.5 A ripple at 12 V, 0.5 A |
| C_in | Input filter cap | 47 µF / 25 V electrolytic | 1 | Close to MOSFET drain |
| C_out | Output filter cap | 47 µF / 25 V electrolytic | 1 | Close to output terminals |
| C6, C7 | HF bypass caps | 100 nF ceramic, placed at each electrolytic | 2 | |

---

## Protection and Safety

| Ref | Description | Value / Part | Qty | Notes |
|---|---|---|---|---|
| F2 | Power-path fuse | 1 A slow-blow (5×20 mm glass) | 1 | Between PSU and converter input |
| SW1 | Emergency stop button | Normally-closed pushbutton | 1 | Connect to PB13 (active-low EXTI) |
| JP1 | Hardware fault header | 2-pin jumper | 1 | PB12 pulled to GND to simulate HW fault |

---

## Passive and Miscellaneous

| Ref | Description | Value / Part | Qty | Notes |
|---|---|---|---|---|
| R_sense2 | Optional secondary shunt | 0.1 Ω / 1 W | 1 | For independent measurement verification |
| LED1 | Status LED | 3 mm red | 1 | Fault indicator |
| R_led | LED resistor | 330 Ω | 1 | 3.3 V → LED → GND |
| PCB / breadboard | Prototype area | | 1 | Use ground plane; separate power/signal |
| Heatsink | MOSFET heatsink | TO-220 clip type | 2 | Required for Q1, Q2 at > 0.5 A |
| Wires | Test leads | 22 AWG silicon | — | Rating ≥ 2× max test current |

---

## Calibration Equipment (not purchased, but required for validation)

| Item | Minimum spec |
|---|---|
| Oscilloscope | 2-channel, ≥ 100 MHz, differential probe capability |
| DMM | 4.5-digit, current and voltage ranges |
| Bench PSU | Adjustable 0–15 V, current limit ≤ 1 A |
| ST-Link V2 | For firmware flashing and SWD debugging |
| USB-RS485 adapter | For Modbus communication from PC |

---

## Substitution Notes

- **STM32F103C8T6**: Blue Pill clones may have a 32.768 kHz (not 8 MHz) crystal
  and only 64 KB Flash. Verify with `swd-info` or CubeIDE device detection.
- **INA199 / INA180**: Many current-sense amps require a specific supply range.
  Verify VOUT does not exceed the ADC reference (3.3 V) at full current.
- **IR2104 vs. IR2110**: IR2104 integrates the SD (shutdown) pin; IR2110 does not.
  Either can be used; adjust dead-time configuration in TIM1 accordingly.
