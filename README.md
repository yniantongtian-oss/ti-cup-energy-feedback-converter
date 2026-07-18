# ti-cup-energy-feedback-converter

**能量回馈变流器负载试验装置开源学习项目**  
**Open-source learning project for an energy-feedback converter load-test apparatus**

[![CI](https://github.com/yniantongtian-oss/ti-cup-energy-feedback-converter/actions/workflows/ci.yml/badge.svg)](https://github.com/yniantongtian-oss/ti-cup-energy-feedback-converter/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
![Status](https://img.shields.io/badge/Status-Experimental-orange)

## 现在可以用什么 | Available Now

仓库提供一个可在 Windows、Linux 和 macOS 编译测试的 **C99 可移植控制与运行时核心**，以及 STM32F103C8T6 Blue Pill 低压参考端口：

- 安全默认输出、解锁/停机和 `IDLE / RUN / FAULT` 状态机
- 电流参考斜率限制、PI 电流环和抗积分饱和
- 过流、母线过压/欠压、过温、无效采样等锁存故障
- ADC 原始值标定、一阶滤波和采样范围检查
- 通信命令看门狗，超时自动停机并把输出归零
- 硬件故障/急停输入接口和安全清故障条件
- CMake 构建、CTest 单元测试和 GitHub Actions 三平台验证
- 主机端 CSV 演示与无第三方依赖的 Python 平均模型
- STM32Cube HAL 参考接入：ADC DMA、TIM1 PWM、1 ms 调度
- MAX485 / Modbus RTU 引脚建议和寄存器表

> 这些内容适合学习、软件验证和隔离低压移植起点；它们不是经过认证或高功率实机验证的完整变流器产品。

## 5 分钟运行 | Quick Start

需要 CMake、C 编译器和 Python 3：

```bash
git clone https://github.com/yniantongtian-oss/ti-cup-energy-feedback-converter.git
cd ti-cup-energy-feedback-converter
cmake -S firmware -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/converter_demo
python3 simulation/simulate.py
```

Windows 多配置生成器的演示程序通常位于 `build/Release/converter_demo.exe`。

## STM32F103C8T6 移植

参考目录：[`platforms/stm32f103-bluepill/`](platforms/stm32f103-bluepill/)

该端口面向此前常见的 Blue Pill + MAX485 方案，提供：

- ADC1 三通道 DMA：电流、母线电压、温度
- TIM1 两路抽象方向 PWM，软件保证不同时输出
- 1 kHz 控制调度
- 急停和硬件故障关断接口
- Modbus 控制层需要调用的 Arm、Disarm、设定值和清故障 API

具体功率拓扑可能需要互补 PWM、死区和独立栅极驱动互锁，不能直接照搬抽象方向输出。引脚、定时器和标定值必须按自己的板卡修改。

通信寄存器见 [`docs/MODBUS_REGISTER_MAP.md`](docs/MODBUS_REGISTER_MAP.md)，portable core 说明见 [`firmware/README.md`](firmware/README.md)。

## 安全说明 | Safety Notice

能量回馈、电力电子和高压实验具有触电、短路、器件爆炸和设备损坏风险。

- 当前代码默认参数仅用于测试逻辑，不能直接作为功率级参数。
- 不要直接接入市电、高压母线或未知功率级。
- 软件保护不能替代保险丝、限流、隔离、急停、硬件比较器和栅极驱动关断。
- 第一次硬件移植必须使用隔离、限流的低压电源，并由具备相关能力的人员复核。
- 若拓扑要求半桥互补驱动，必须使用定时器死区、Break 输入和硬件互锁，不能把 CH1/CH2 当作普通方向信号直接驱动桥臂。

## 仓库结构 | Repository Structure

```text
firmware/                         可移植控制核心、运行时、演示和测试
platforms/stm32f103-bluepill/     STM32Cube HAL 低压参考端口
simulation/                       无第三方依赖的平均模型演示
docs/                             架构、通信、硬件、路线图和贡献文档
hardware/                         硬件说明与后续参考设计区域
examples/                         后续实验和上位机示例
.github/workflows/                 Windows/macOS/Ubuntu 自动构建与测试
```

## 尚未伪装成“已完成”的部分

以下内容必须在确定实际题目指标、功率拓扑、器件和传感器后完成，仓库不会用虚构参数代替：

- 可制造的原理图、PCB、Gerber、BOM 与磁性元件设计
- 与具体栅极驱动器对应的互补 PWM、死区和 Break 配置
- 真实传感器的零点、增益、温漂与误差标定
- 低压台架、额定功率、效率、温升、动态和故障实测数据
- 根据当年官方赛题确认的指标、接口和合规性

路线图见 [`docs/ROADMAP.md`](docs/ROADMAP.md)，贡献规范见 [`docs/CONTRIBUTING.md`](docs/CONTRIBUTING.md)。

## License

本项目采用 [MIT License](LICENSE)。使用者需自行完成规则合规、参数设计、硬件验证和安全评估。
