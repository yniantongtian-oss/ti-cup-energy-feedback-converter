# Release acceptance

This repository separates **portable software validation** from **hardware validation**. A green CI run is evidence for the former only.

## Software release gate

A software-only prerelease may be created when:

- CI passes on the supported host platforms;
- firmware core tests and simulation smoke tests pass;
- the STM32 reference adapter builds or is statically validated as documented;
- public APIs, Modbus register mapping, and safety defaults are documented;
- the release notes explicitly state that no power-stage hardware validation is implied.

## Hardware-qualified release gate

Do not call a release hardware-qualified, competition-ready, or validated until the repository contains traceable evidence for all applicable items below:

- exact competition specification/revision;
- finalized topology and component/BOM revision;
- manufacturable schematic, PCB, Gerber and assembly package;
- PWM/dead-time/Break configuration matching the actual gate driver;
- current/voltage/temperature calibration records and error table;
- low-voltage current-limited open-loop test records;
- low-current closed-loop test records and PI parameters;
- communication timeout/CRC/reconnect/reset regression;
- raw CSV and oscilloscope captures with instrument metadata;
- firmware commit SHA for every test record;
- thermal-rise, repeatability, long-duration and fault-regression records.

Evidence belongs under `hardware/validation/` and must be referenced from a manifest. Do not replace missing measurements with simulated or example values.

## Safety boundary

Until the hardware-qualified gate is complete, do not connect the reference project directly to mains, a high-voltage DC bus, or an unreviewed power stage. Use isolation and current-limited low-voltage supplies for first hardware bring-up.
