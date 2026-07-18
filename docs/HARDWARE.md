# Hardware Reference | 硬件参考

This document describes the hardware platform and requirements for the energy feedback converter project.

## Recommended Hardware Platform | 推荐硬件平台

- **Main Controller**: STM32F103C8T6 / STM32F407VGT6
- **Power Stage Topology**: Bidirectional Buck-Boost or similar
- **Current Sensing**: Hall sensor or shunt resistor + amplifier
- **Voltage Sensing**: Resistive divider + op-amp
- **Temperature Sensing**: NTC thermistor
- **Driver**: IR2110 / similar half-bridge driver
- **Communication**: CAN or RS485 (optional)

## Key Design Considerations | 关键设计考虑

- High-side and low-side current sampling
- Fast protection response (< 10us)
- Proper PCB layout for power and signal separation
- Thermal management for continuous operation

## Reference Schematic | 参考原理图

Schematic files will be placed in the `hardware/` directory (Altium / 立创 EDA format).

## Safety Notes | 安全说明

- High voltage and high current involved — use proper isolation and protection
- Always test at low power first
- Follow competition safety rules

> Hardware design is currently in progress. Contributions are welcome!