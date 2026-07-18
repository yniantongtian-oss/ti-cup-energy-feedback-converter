# Modbus RTU Register Map

本寄存器表用于 STM32F103C8T6 + MAX485 参考实现。设备作为 Modbus RTU 从站，上位机作为主站。

## 默认通信参数

- 从站地址：`1`
- 波特率：`9600`（可改为 `19200`）
- 数据格式：8N1
- 字节序：每个 16 位寄存器使用 Modbus 大端传输
- 写命令超时：默认 500 ms；超时后自动停机并把 PWM 归零

## Holding Registers（功能码 0x03 / 0x06 / 0x10）

| 地址 | 名称 | 类型/单位 | 访问 | 说明 |
|---:|---|---|---|---|
| 0 | Control | bit field | R/W | bit0=Arm，bit1=Disarm，bit2=ClearFault，bit3=EmergencyStop |
| 1 | Current reference | int16, mA | R/W | 正值正向，负值反向；内部仍受限幅与斜率限制 |
| 2 | Command timeout | uint16, ms | R/W | 建议 100–2000 ms |
| 3 | Current limit | uint16, mA | R/W | 软件限幅，不等同于硬件过流保护 |
| 4 | Current trip | uint16, mA | R/W | 必须大于 Current limit |
| 5 | Bus minimum | uint16, 0.01 V | R/W | 欠压锁存阈值 |
| 6 | Bus maximum | uint16, 0.01 V | R/W | 过压锁存阈值 |
| 7 | Temperature trip | int16, 0.1 °C | R/W | 过温锁存阈值 |
| 8 | Kp | uint16, 1e-4 | R/W | 写入前必须停机 |
| 9 | Ki | uint16, 1e-3 | R/W | 写入前必须停机 |

## Input Registers（功能码 0x04）

| 地址 | 名称 | 类型/单位 | 说明 |
|---:|---|---|---|
| 0 | State | uint16 | 0=IDLE，1=RUN，2=FAULT |
| 1 | Fault flags low | uint16 | 故障位低 16 位 |
| 2 | Fault flags high | uint16 | 故障位高 16 位 |
| 3 | Measured current | int16, mA | 标定和滤波后的电流 |
| 4 | Bus voltage | uint16, 0.01 V | 标定和滤波后的母线电压 |
| 5 | Temperature | int16, 0.1 °C | 标定和滤波后的温度 |
| 6 | Duty command | int16, 1e-4 | -10000 至 10000 |
| 7 | Requested current | int16, mA | 通信层最近一次请求值 |
| 8 | Ramped current | int16, mA | 斜率限制后的内部参考 |
| 9 | Uptime low | uint16 | 毫秒计数低 16 位 |
| 10 | Uptime high | uint16 | 毫秒计数高 16 位 |
| 11 | Firmware version | uint16 | 例如 `0x0100` 表示 1.0 |

## 控制要求

1. `Arm` 只在无故障、急停释放、采样有效时生效。
2. `Disarm` 必须立即把两个方向 PWM 都设为 0。
3. `ClearFault` 只在停机且测量恢复正常时生效。
4. `EmergencyStop` 必须锁存，且不能仅通过远程命令解除；应要求本地人工复位。
5. 每次成功写入参考电流或控制命令都刷新通信看门狗。
6. 参数写入 Flash 前应增加版本号、范围检查和 CRC；禁止在 RUN 状态直接修改控制参数。

## 故障位建议

| 位 | 含义 |
|---:|---|
| 0 | 过流 |
| 1 | 母线过压 |
| 2 | 母线欠压 |
| 3 | 过温 |
| 4 | 无效采样/ADC 越界 |
| 5 | 控制周期异常 |
| 6 | 硬件故障输入 |
| 7 | 急停 |
| 8 | 通信超时 |
| 9 | 参数 CRC 错误 |

当前 portable core 已实现前六类软件行为；硬件故障、急停、通信超时和参数持久化由平台层负责映射。
