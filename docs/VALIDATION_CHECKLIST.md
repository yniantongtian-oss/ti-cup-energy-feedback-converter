# Low-Voltage Validation Checklist | 低压验证清单

> **Safety rule**: Do NOT connect mains or high-voltage bus until every item in
> Phase 0–4 is signed off with measured data.  All entries must be filled with
> actual instrument readings, not default values.

---

## How to use this checklist

1. Work through the phases in order.  Failure at any phase is a stop condition.
2. Record instrument models and serial numbers in the header below.
3. Record firmware SHA (`git describe --tags --always --dirty`) for every test run.
4. Save raw CSV, oscilloscope screenshots and multimeter photos alongside this file.

**Firmware SHA**: `________________`  
**Test date**: `________________`  
**Tester**: `________________`

**Instruments**:

| Role | Model | S/N | Last calibration |
|---|---|---|---|
| Oscilloscope | | | |
| DMM (voltage) | | | |
| DMM (current) | | | |
| Bench PSU | | | |
| Function generator (optional) | | | |

---

## Phase 0 – No-power safety baseline

Perform all checks with the power stage **completely disconnected**.
Only the STM32 Blue Pill and its 3.3 V / 5 V supply are powered.

| # | Test | Pass criterion | Result | Notes |
|---|---|---|---|---|
| 0.1 | Read PA8, PA9 with DMM immediately after power-on | Both pins < 0.5 V (PWM off by default) | | |
| 0.2 | Oscilloscope on PA8 + PA9 for 10 s at idle | No pulse on either channel | | |
| 0.3 | Pull PB12 (HW\_FAULT) to GND, check state via Modbus IR 0 | State = FAULT (2) within 1 control tick | | |
| 0.4 | Pull PB13 (ESTOP) to GND, check IR 0 | State = FAULT (2), emergency latched | | |
| 0.5 | Attempt Arm (HR 0 = 0x01) while ESTOP active | Arm rejected; state remains FAULT | | |
| 0.6 | Release ESTOP (PB13 high), send ClearFault, re-arm | State transitions IDLE → RUN only after clear | | |
| 0.7 | Send EmergencyStop (HR 0 = 0x08) via Modbus | State = FAULT; only local power-cycle resets latch | | |
| 0.8 | Verify both PA8 and PA9 are zero after emergency stop | Oscilloscope confirms both channels silent | | |

---

## Phase 1 – Simulated ADC validation

Use a bench PSU or function generator to drive PA0, PA1, PA2 through
voltage dividers.  Do NOT connect any real power stage.

| # | Test | Pass criterion | Result | Notes |
|---|---|---|---|---|
| 1.1 | Drive PA0 to midscale (≈ 1.65 V); read IR 3 via Modbus | Calibrated current = 0 A ± 5 mA | | |
| 1.2 | Drive PA0 to 0 V; read IR 3 | Current reading = `−(offset_a)` ± 5 mA | | |
| 1.3 | Drive PA0 to 3.3 V; check for overflow or clamp | ADC = 4095, value within range | | |
| 1.4 | Drive PA1 to produce 12.00 V calibrated bus; read IR 4 | IR 4 = 1200 ± 5 (centivolt units) | | |
| 1.5 | Drive PA2 to produce 25.0 °C calibrated temp; read IR 5 | IR 5 = 250 ± 5 (deci-°C units) | | |
| 1.6 | Set bus below min threshold (HR 5); verify UNDERVOLTAGE fault | IR 1 bit 3 set; PA8 and PA9 → 0 | | |
| 1.7 | Set bus above max threshold (HR 6); verify OVERVOLTAGE fault | IR 1 bit 2 set | | |
| 1.8 | Set current ADC to simulate current > trip (HR 4); verify OVERCURRENT | IR 1 bit 1 set; duty → 0 | | |
| 1.9 | Set temperature ADC to simulate temp > trip (HR 7) | IR 1 bit 4 set | | |
| 1.10 | Disconnect PA0 (sample\_valid = false after ADC stalls) | IR 1 bit 0 set within `ADC_FRESHNESS_TICKS_MAX` ms | | |

Record calibration error table (fill in actual measured vs. reported values):

| Signal | Input stimulus | Expected reading | Actual reading | Error |
|---|---|---|---|---|
| Current | 0 A (PA0 = 1.65 V) | 0 A | | |
| Current | +1 A | 1000 mA | | |
| Bus voltage | 12.00 V | 1200 (×0.01 V) | | |
| Bus voltage | 5.00 V | 500 (×0.01 V) | | |
| Temperature | 25.0 °C | 250 (×0.1 °C) | | |
| Temperature | 50.0 °C | 500 (×0.1 °C) | | |

---

## Phase 2 – Modbus communication validation

| # | Test | Pass criterion | Result | Notes |
|---|---|---|---|---|
| 2.1 | Send FC 03 (read HR 0–9) | Valid response with correct CRC | | |
| 2.2 | Send FC 04 (read IR 0–11) | All fields plausible; IR 11 = firmware version | | |
| 2.3 | Send FC 06 write current ref (HR 1 = 500) | IR 7 = 500 within 1 ms | | |
| 2.4 | Send FC 10 write multiple (HR 0–1) | Echo response correct; control executed | | |
| 2.5 | Send request with wrong CRC | No response (slave silently discards) | | |
| 2.6 | Send request to wrong slave address (≠ 1) | No response | | |
| 2.7 | Wait > command\_timeout\_ms without any write | IR 0 = IDLE (1) and duty = 0 | | |
| 2.8 | Reset MCU, reconnect within 1 s, re-arm | Normal operation resumes | | |
| 2.9 | Write HR 8 (Kp) while state = RUN | Exception response 0x02 | | |
| 2.10 | Write HR 8 (Kp) while state = IDLE | Accepted (readback matches) | | |

---

## Phase 3 – Open-loop low-voltage validation (5–12 V bench PSU)

Connect a **current-limited** bench PSU (limit ≤ 0.5 A) to the converter
input via a series resistor (≥ 10 Ω).  No motor or reactive load.

| # | Test | Pass criterion | Result | Notes |
|---|---|---|---|---|
| 3.1 | Power on, verify no PWM at idle | Oscilloscope: both channels silent | | |
| 3.2 | Arm and set current ref = 100 mA | Duty ramps up smoothly; no shoot-through on scope | | |
| 3.3 | Scope PA8 and PA9 simultaneously | Never both non-zero at the same time | | |
| 3.4 | Set current ref = 0, verify duty ramps to 0 | Slew limit respected; no step change | | |
| 3.5 | Trigger overcurrent by lowering HR 4 below actual current | Fault latch; duty → 0 immediately | | |
| 3.6 | Record: Vin, Iout, duty, temp at steady state | Fill table below | | |
| 3.7 | Run for 30 min; log temp and current every 5 min | Temperature rise < 30 °C above ambient | | |

Steady-state measurements (Phase 3.6):

| Parameter | Measured value | Instrument |
|---|---|---|
| Input voltage (V) | | |
| Output current (A) | | |
| Duty command (IR 6 / 10000) | | |
| Board temperature (°C) | | |
| Ambient temperature (°C) | | |

---

## Phase 4 – Closed-loop validation

Perform after Phase 3 passes completely.

| # | Test | Pass criterion | Result | Notes |
|---|---|---|---|---|
| 4.1 | Enable closed loop; set Iref = 200 mA; scope current waveform | Settled within 100 ms; overshoot < 20% | | |
| 4.2 | Step Iref 200 mA → 500 mA; record rise time | Rise ≤ slew limit; no oscillation | | |
| 4.3 | Record PI parameters that achieved stable operation | Fill table below | | |
| 4.4 | Save raw CSV from Modbus polling (1 Hz, 60 s) | No missing packets; all values in range | | |
| 4.5 | Multi-reset test: power-cycle 5 times; verify proper restart | No residual duty after each reset | | |
| 4.6 | Flash-save parameters (AppFlashParams\_Save); power-cycle; verify reload | Readback matches saved values and CRC passes | | |

PI parameters at stable operation:

| Parameter | Value | Register | Notes |
|---|---|---|---|
| Kp | | HR 8 (×1e-4) | |
| Ki | | HR 9 (×1e-3) | |
| Current limit (A) | | HR 3 (mA) | |
| Current trip (A) | | HR 4 (mA) | |
| Slew rate (A/s) | | (firmware default) | |
| Integrator limit | | (firmware default) | |

---

## Sign-off

| Phase | Pass / Fail | Date | Tester |
|---|---|---|---|
| 0 – No-power safety | | | |
| 1 – ADC validation | | | |
| 2 – Modbus comms | | | |
| 3 – Open-loop low-V | | | |
| 4 – Closed-loop | | | |

> All phases passed → proceed to system integration.
> Any phase failed → fix root cause, re-run failed phase and all subsequent phases.
