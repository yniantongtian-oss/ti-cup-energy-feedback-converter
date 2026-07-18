# ti-cup-energy-feedback-converter

**能量回馈变流器负载试验装置开源学习项目**  
**Open-source learning project for an energy-feedback converter load-test apparatus**

[![CI](https://github.com/yniantongtian-oss/ti-cup-energy-feedback-converter/actions/workflows/ci.yml/badge.svg)](https://github.com/yniantongtian-oss/ti-cup-energy-feedback-converter/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
![Status](https://img.shields.io/badge/Status-Experimental-orange)

## 现在可以用什么 | Available Now

仓库现已提供一个可在 Windows、Linux 和 macOS 主机上编译测试的 **C99 可移植控制核心**，以及一个无需第三方依赖的 Python 平均模型演示：

- 安全默认输出、解锁/停机和 `IDLE / RUN / FAULT` 状态机
- 电流参考斜率限制、PI 电流环和抗积分饱和
- 过流、母线过压/欠压、过温、无效采样等锁存故障
- CMake 构建、CTest 单元测试和 GitHub Actions
- 主机端 CSV 演示与 Python 仿真输出

> 这些内容适合学习、软件验证和低压移植起点；它们不是经过认证或实机验证的完整变流器产品。

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

详细移植说明见 [`firmware/README.md`](firmware/README.md)。

## 安全说明 | Safety Notice

能量回馈、电力电子和高压实验具有触电、短路、器件爆炸和设备损坏风险。

- 当前代码默认参数仅用于测试逻辑，不能直接作为功率级参数。
- 不要直接接入市电、高压母线或未知功率级。
- 软件保护不能替代保险丝、限流、隔离、急停、硬件比较器和栅极驱动关断。
- 第一次硬件移植必须使用隔离、限流的低压电源，并由具备相关能力的人员复核。

## 仓库结构 | Repository Structure

```text
firmware/              可移植 C99 控制核心、演示和测试
simulation/            无第三方依赖的平均模型演示
docs/                  架构、硬件、路线图和贡献文档
hardware/              硬件设计说明与后续参考设计区域
examples/              后续平台适配和实验示例
.github/workflows/      自动构建与测试
```

## 下一步 | Roadmap

- STM32CubeIDE 平台适配层与固定周期调度示例
- ADC 标定、PWM 映射和硬件故障输入接口
- 更完整的离散模型、参数文件和测试数据
- 低压硬件参考设计及可复现实验记录

路线图见 [`docs/ROADMAP.md`](docs/ROADMAP.md)，贡献规范见 [`docs/CONTRIBUTING.md`](docs/CONTRIBUTING.md)。

## License

本项目采用 [MIT License](LICENSE)。使用者需自行完成规则合规、参数设计、硬件验证和安全评估。
