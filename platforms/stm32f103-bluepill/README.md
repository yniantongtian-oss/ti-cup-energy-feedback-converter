# STM32F103C8T6 Blue Pill 低压参考端口

本目录给出把 `firmware/` 中可测试控制核心接入 STM32Cube HAL 工程的参考方式。目标是让使用者能快速建立**隔离、限流、低压**原型，而不是提供可直接接市电的功率产品。

## 平台层模块

| 文件 | 功能 |
|---|---|
| `Core/Inc/app_converter.h` / `Core/Src/app_converter.c` | ADC + TIM1 + 故障输入；1 ms 调度入口；ADC 采样新鲜度检测 |
| `Core/Inc/app_modbus.h` / `Core/Src/app_modbus.c` | Modbus RTU 从站（FC 03/04/06/10）；寄存器表与控制核心映射 |
| `Core/Inc/app_flash_params.h` / `Core/Src/app_flash_params.c` | Flash 参数持久化（magic + version + CRC-32）；范围检查 |

所有平台层模块都依赖 STM32 HAL，不参与 host-side CMake 测试；  
可移植核心的纯 C 测试在 `firmware/tests/` 中运行。

## 推荐工具链

- MCU：STM32F103C8T6（Blue Pill）
- IDE：STM32CubeIDE 或 Keil MDK
- 下载器：ST-Link
- 通信：USART1 + MAX485，Modbus RTU，9600/19200 bps，8N1
- 控制周期：1 kHz
- PWM：TIM1，建议 20 kHz
- ADC：ADC1 + DMA，3 通道循环采样

## 参考引脚

| 功能 | 引脚 | 外设 |
|---|---|---|
| 电流采样 | PA0 | ADC1_IN0 |
| 母线电压采样 | PA1 | ADC1_IN1 |
| 温度采样 | PA2 | ADC1_IN2 |
| 正向 PWM | PA8 | TIM1_CH1 |
| 反向 PWM | PA9 | TIM1_CH2（与 USART1 TX 冲突时改用 PB14/TIM1_CH2N 或其他定时器） |
| RS485 TX | PB10 | USART3_TX |
| RS485 RX | PB11 | USART3_RX |
| RS485 DE/RE | PB1 | GPIO 输出 |
| 硬件故障输入 | PB12 | GPIO EXTI，低有效 |
| 急停输入 | PB13 | GPIO EXTI，低有效 |

> Blue Pill 克隆板的晶振、USB 上拉和 Flash 容量可能不一致。首次使用必须确认板卡型号和时钟。

## CubeMX 配置

1. ADC1 使用 Scan Conversion + Continuous Conversion + DMA Circular。
2. 通道顺序固定为 IN0、IN1、IN2，采样时间建议从 55.5 cycles 起步。
3. TIM1 设置为 20 kHz PWM；上电时两个通道占空比必须为 0。
4. USART3 设置为 9600 或 19200 bps、8N1。
5. PB12/PB13 配置为上拉输入；外部硬件故障或急停触发时立即关断 PWM。
6. 使用 TIM2 或 SysTick 产生 1 ms 调度。

## 集成步骤

1. 新建 STM32CubeIDE/Keil 工程并生成 HAL 初始化代码。
2. 把 `firmware/include/*.h` 与 `firmware/src/converter*.c` 加入工程。
3. 把本目录的 `Core/Inc/*.h` 和 `Core/Src/*.c` 全部加入工程。
4. 在外设初始化完成后调用 `AppConverter_Init()` 和 `AppModbus_Init(slave_addr)`。
5. 在 `HAL_ADC_ConvCpltCallback()` 中调用 `AppConverter_AdcDmaUpdate()`。
6. 在 USART3 接收中断（或 `HAL_UART_RxCpltCallback`）中调用 `AppModbus_ByteReceived(byte)`。
7. 在 1 ms 调度器中依次调用 `AppConverter_1msTask(now_ms)` 和 `AppModbus_1msTick(now_ms)`。
8. 在硬件故障和急停 EXTI 中调用 `AppConverter_EmergencyStop()`。
9. 上电时尝试 `AppFlashParams_Load()`；若失败则使用默认值并调用 `AppFlashParams_Save()`。

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

完整逐项验证清单见 [`../../docs/VALIDATION_CHECKLIST.md`](../../docs/VALIDATION_CHECKLIST.md)。  
详细通信寄存器见 [`../../docs/MODBUS_REGISTER_MAP.md`](../../docs/MODBUS_REGISTER_MAP.md)。  
参考 BOM 见 [`../../hardware/BOM.md`](../../hardware/BOM.md)。
