# STM32F103C8T6 Blue Pill 低压参考端口

本目录给出把 `firmware/` 中可测试控制核心接入 STM32Cube HAL 工程的参考方式。目标是让使用者能快速建立**隔离、限流、低压**原型，而不是提供可直接接市电的功率产品。

## 推荐工具链

- MCU：STM32F103C8T6（Blue Pill）
- IDE：STM32CubeIDE 或 Keil MDK
- 下载器：ST-Link
- 通信：USART1 + MAX485，Modbus RTU，9600/19200 bps，8N1
- 电流环：TIM1 中心对齐 **~20 kHz**，注入组采样（I + U1）
- 慢路径：1 kHz（母线/温度规则组 DMA、安全、Modbus）——**不是**电流环采样率
- PWM：TIM1，建议 20 kHz，中心对齐
- ADC：规则组 DMA = 母线+温度；注入组 = IN0 电流 + IN3 U1，触发 = TIM1 **OCxREF**（禁止 Update）

## 参考引脚

| 功能 | 引脚 | 外设 |
|---|---|---|
| 电流采样 | PA0 | ADC1_IN0 |
| 母线电压采样 | PA1 | ADC1_IN1 |
| 温度采样 | PA2 | ADC1_IN2 |
| 植物侧 U1 | PA3 | ADC1_IN3（软件通道，板上可暂无探头） |
| 正向 PWM | PA8 | TIM1_CH1 |
| 反向 PWM | PA9 | TIM1_CH2（与 USART1 TX 冲突时改用 PB14/TIM1_CH2N 或其他定时器） |
| RS485 TX | PB10 | USART3_TX |
| RS485 RX | PB11 | USART3_RX |
| RS485 DE/RE | PB1 | GPIO 输出 |
| 硬件故障输入 | PB12 | GPIO EXTI，低有效 |
| 急停输入 | PB13 | GPIO EXTI，低有效 |

> Blue Pill 克隆板的晶振、USB 上拉和 Flash 容量可能不一致。首次使用必须确认板卡型号和时钟。

## CubeMX 配置（步骤 B）

1. **TIM1**：中心对齐模式 1 或 2，PWM ~20 kHz；CH1/CH2（或互补）输出。另选 **CH4** 作安静采样点：CCR4≈ARR（或 0），TRGO/`OC4REF` 给 ADC 注入触发。**不要**用 Update 触发（中心对齐下约 2×PWM）。
2. **ADC1 注入组**：长度 2；Rank1=`IN0` 电流，Rank2=`IN3` U1(PA3)；外部触发 = TIM1_TRGO 或 TIM1_CH4；使能注入完成中断。在 `HAL_ADCEx_InjectedConvCpltCallback` 里调用 `AppConverter_CurrentLoopFromInjected(...)`。
3. **ADC1 规则组**：Scan + Continuous + DMA Circular，通道 `IN1` 母线、`IN2` 温度（仅慢变量）。采样时间建议从 55.5 cycles 起步。
4. USART3：9600 或 19200 bps、8N1（Modbus）。
5. PB12/PB13：上拉输入，软件故障/急停。**过流硬件掐门预定 PA6=TIM1_BKIN（步骤 F，本步不配 BDTR）**。
6. TIM2 或 SysTick：1 ms 只调 `AppConverter_1msTask`（刷新慢采样缓存），**不要**在 1 ms 里跑电流 PI 或写占空比。

## 集成步骤

1. 新建 STM32CubeIDE/Keil 工程并生成 HAL 初始化代码。
2. 把 `firmware/include/*.h` 与 `firmware/src/converter*.c` 加入工程。
3. 把本目录的 `Core/Inc/app_converter.h` 和 `Core/Src/app_converter.c` 加入工程。
4. 在外设初始化完成后调用 `AppConverter_Init()`。
5. 启动规则 DMA + 注入中断后：注入完成回调跑电流环；每 1 ms 只调 `AppConverter_1msTask()`。
6. 在硬件故障和急停 EXTI 中调用 `AppConverter_EmergencyStop()`。
7. 在 Modbus 写寄存器回调中调用设置参考值、解锁、停机或清故障接口。

## 必须实现的硬件保护

软件保护不能替代以下措施：

- 限流电源或串联功率电阻
- 栅极驱动硬件关断
- 比较器级过流保护
- 急停按钮
- 保险丝
- 隔离调试和差分测量
- PWM 上电默认关闭

## 首次低压验证顺序

1. 不连接功率级，只检查 PWM 默认关闭。
2. 用电位器或信号源模拟 ADC，确认标定、滤波和越限故障。
3. 用示波器确认正反向 PWM 不会同时有效。
4. 使用 5–12 V 限流电源和纯电阻负载进行开环验证。
5. 最后才进行闭环调参，并从极低电流限值开始。

详细通信寄存器见 [`../../docs/MODBUS_REGISTER_MAP.md`](../../docs/MODBUS_REGISTER_MAP.md)。


## 已锁定、本步未实现的保护脚（F）

过流硬件掐门预定 **PA6 = TIM1_BKIN**，低有效，BKP=0，AOE=0。PB12 仍是 GPIO 故障输入，**不能替代 BKIN**。比较器/nFAULT 器件未上板时固件仍按 PA6 预留，不改板。本 PR 只做 A（U1 前馈），不配 BDTR。


## 同步（步骤 C）

- 同步源：**SOGI-PLL 锁植物侧 U1**（注入采样）。禁止开环斜坡 / RAMPGEN 当角度源。
- `converter_runtime` 在每次电流环节拍更新 PLL；`pll.locked==false` 时 `duty=0` 且 `pwm_released=false`。
- 平台 ISR 再次检查 `pwm_released` 后才写 CCR。


## Park / Iq=0（步骤 D）

- 电流 SOGI 出 `i_beta`，与 PLL `theta` 做 Park。
- `Id_ref` = 电流给定（斜率限制后），`Iq_ref = 0`。
- 双 PI → 反 Park → `u_alpha`，再 `(u_alpha+U1)/Vbus`。不堆谐波 PR。
