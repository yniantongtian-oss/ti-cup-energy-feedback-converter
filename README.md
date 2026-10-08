# Energy-Feedback Converter Test Platform

**An experimental, open-source learning project for an energy-feedback converter load-test apparatus.**

[![CI](https://github.com/yniantongtian-oss/ti-cup-energy-feedback-converter/actions/workflows/ci.yml/badge.svg)](https://github.com/yniantongtian-oss/ti-cup-energy-feedback-converter/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
![Status](https://img.shields.io/badge/Status-Experimental-orange)

## Implemented software

The repository provides a portable C99 control and runtime core that builds on Windows, Linux, and macOS, plus a low-voltage STM32F103C8T6 Blue Pill reference port.

- Safe default outputs and the `IDLE / RUN / FAULT` state machine, with arming, disarming and shutdown controls.
- Current-reference slew limiting, a PI loop and anti-windup protection.
- Latched faults for overcurrent, DC-bus over/undervoltage, overtemperature and invalid sensor readings.
- ADC calibration, first-order filtering and range checks.
- A communication command watchdog that stops control and zeros the output after a timeout.
- Interfaces for hardware fault and emergency-stop shutdown.
- CMake builds, CTest unit tests and cross-platform GitHub Actions.
- A host-side CSV demonstration and dependency-free Python averaged model.
- STM32Cube HAL reference wiring for ADC DMA, TIM1 PWM and a 1 ms scheduling tick.
- MAX485/Modbus RTU pin recommendations, register map and host frame decoder.
- Twelve input-register telemetry decoding with CRC, signed values, unit conversions and word-order checks.

**Scope:** this is suitable for learning, software validation and isolated low-voltage porting. It is not a certified or high-power experimentally validated converter.

## Quick start

Requires CMake, a C compiler and Python 3:

```bash
git clone https://github.com/yniantongtian-oss/ti-cup-energy-feedback-converter.git
cd ti-cup-energy-feedback-converter
cmake -S firmware -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/converter_demo
python3 simulation/simulate.py
```

On Windows with a multi-configuration generator, the demonstration executable is typically at `build/Release/converter_demo.exe`.

## STM32F103C8T6 reference port

See [platforms/stm32f103-bluepill](platforms/stm32f103-bluepill/). The low-voltage reference supports:

- Three-channel ADC1 DMA for current, bus voltage and temperature.
- Two abstract direction-PWM signals from TIM1, with a software mutual-exclusion gate.
- A 1 kHz control scheduler.
- Emergency-stop and hardware-fault input interfaces.
- Arm, disarm, setpoint and fault-clear functions for the protocol layer.

An actual power stage may require complementary PWM, dead time, and dedicated gate-driver interlocks. Do **not** wire the example direction outputs directly to a half bridge. Pin assignments, sensor calibration, PWM configuration and protection settings must be adapted to the target circuit.

The [register map](docs/MODBUS_REGISTER_MAP.md) and [portable firmware guide](firmware/README.md) document the interfaces. Host examples live in [examples/host](examples/host/). The original frame utility was extracted from [electronics resources](https://github.com/yniantongtian-oss/ti-cup-resources); source revisions and license references are in [tools/sources.json](tools/sources.json).

A functional STM32 Modbus slave and production serial integration have **not** been implemented or verified.

## Electrical safety

Regenerative power electronics and high-voltage tests can cause electric shock, short circuits, component rupture, fire, and equipment damage.

- Do not use default software constants as real power-stage ratings.
- Do not connect the reference design directly to mains, a high-voltage DC bus, or an unverified power stage.
- Software protection does not replace fuses, isolation, current limiting, emergency stop, hardware comparators, and independent gate-driver shutdown.
- Initial hardware tests require an isolated current-limited low-voltage supply and review by a qualified engineer.
- Half-bridge designs need verified dead time, timer break inputs, and hardware interlocks; ordinary PWM signals cannot be treated as safe complementary control outputs.

## Repository layout

```text
firmware/                       Portable control code, runtime, demo and tests
platforms/stm32f103-bluepill/   Low-voltage STM32Cube HAL reference
simulation/                     Dependency-free averaged model
docs/                           Architecture, communication, safety and roadmaps
hardware/                       Hardware notes and future reference designs
examples/                       Host integration and bench examples
.github/workflows/               Windows/macOS/Ubuntu builds and tests
```

## Not yet validated

The following require an actual competition specification, power topology, component selection and measurement evidence:

- Manufacturable schematics, PCB, Gerbers, bill of materials and magnetics.
- Gate-driver-specific complementary PWM, dead time and break handling.
- Sensor offset, gain, temperature drift and measurement uncertainty.
- Low-voltage bench, full-power efficiency, temperature rise, transient and fault measurements.
- Requirements confirmed against the applicable competition or hardware safety rules.

See the [roadmap](docs/ROADMAP.md) and [contribution guide](docs/CONTRIBUTING.md). No fabricated hardware parameters or measurement data are presented as verified results.

## License

[MIT License](LICENSE). Users are responsible for reviewing component ratings, physical safeguards, local rules and real hardware validation.
