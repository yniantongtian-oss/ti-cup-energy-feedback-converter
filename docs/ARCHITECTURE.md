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

- Inner current loop (fast)
- Outer voltage loop
- Energy feedback / recovery logic
- Mode switching logic (buck, boost, bidirectional)

## Safety & Protection | 安全与保护

Multi-layer protection with fast hardware + software response.

> Architecture is still evolving. Feedback and suggestions are welcome.