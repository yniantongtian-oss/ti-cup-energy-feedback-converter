# System Architecture | 系统架构

This document describes the overall software and hardware architecture of the energy feedback converter system.

## High-Level Architecture | 高层架构

```
                    +-------------------+
                    |   User Interface  |
                    |  (HMI / PC Tool)  |
                    +---------+---------+
                              |
                    +---------v---------+
                    |   Communication   |  <--- CAN / RS485 / UART
                    |     Layer         |
                    +---------+---------+
                              |
          +-------------------+-------------------+
          |                                       |
+---------v---------+               +---------v---------+
|   Control Layer   |               |   Protection      |
|   (Main Control)  |               |   Layer           |
+---------+---------+               +---------+---------+
          |                                   |
+---------v---------+               +---------v---------+
|   PWM Generation  |               |   Fault Handling  |
|   & Modulation    |               |   & Recovery      |
+---------+---------+               +-------------------+
          |
+---------v---------+
|   Power Stage     |
|   (Hardware)      |
+-------------------+
```

## Software Layers | 软件层

1. **Application Layer** — High-level control strategies, mode management
2. **Control Layer** — PID, current/voltage loops, MPPT-like algorithms
3. **Driver Layer** — PWM, ADC, timers, communication drivers
4. **HAL / BSP** — STM32 HAL or LL drivers

## Key Control Loops | 关键控制循环

- Inner current loop (fast). The portable core runs a PID whose default derivative gain is zero, with slew limiting and conditional-integration anti-windup.
- Single-phase SOGI-PLL. It is host-tested and available for a future grid-voltage sample. The Blue Pill 1 ms task does not call it.
- Outer voltage loop, mode switching, and a full bidirectional modulator are not implemented.

## Host digital twin | 主机数字孪生

`simulation/twin/` repeats the current-loop, protection, and PLL math for gain trials and SVG plots. `simulation/tune.py` is the machine-readable interface: one JSON object on stdout, CSV and SVG artifacts on disk. Trip decisions use the instantaneous calibrated sample; the runtime's first-order filter is only the regulator feedback.

## Safety & Protection | 安全与保护

Multi-layer protection with fast hardware + software response.

> Architecture is still evolving. Feedback and suggestions are welcome.