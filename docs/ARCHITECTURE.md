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

- Inner current loop (PI). Output is voltage `u_i`, not duty.
- Duty: `(u_i + U1) / Vbus`. Feedforward default ON. U1 is plant-side voltage (PA3/ADC1_IN3).
- Current loop rate: **~20 kHz** (TIM1 center-aligned + injected ADC). 1 kHz = slow path only.
- Outer voltage loop — **not in this step**
- Sync: **SOGI-PLL on U1**; PWM gated until lock; **no RAMPGEN**.
- Park + **Iq=0** (dual PI on Id/Iq); **no harmonic PR**.
- Bring-up SM: OPEN_LOOP → CURRENT_DC → OUTER_V; bus cmd slow ramp.
- TIM1 BKIN on **PA6**: BKE=1, BKP=0, AOE=0; MOE cleared on break before ISR.

## Safety & Protection | 安全与保护

Multi-layer protection with fast hardware + software response.

> Architecture is still evolving. Feedback and suggestions are welcome.