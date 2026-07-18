# ti-cup-energy-feedback-converter

**能量回馈变流器负载试验装置开源学习项目**  
**Open-source learning project for an energy-feedback converter load-test apparatus**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
![Status](https://img.shields.io/badge/Status-Pre--alpha-orange)
![Phase](https://img.shields.io/badge/Phase-Documentation%20%26%20Planning-blue)

## 项目定位 | Project Scope

本仓库面向功率电子、嵌入式控制和大学生电子设计竞赛学习，整理能量回馈变流器系统的架构、控制思路、保护策略、仿真计划与工程文档。

> **当前状态：Pre-alpha。** 仓库目前以文档和项目规划为主，尚未提供可直接编译、烧录或用于真实功率平台的完整固件、原理图和仿真模型。

This repository currently focuses on documentation and project planning. It does **not yet** provide a complete, verified firmware project, schematic, PCB design, or simulation model.

## 安全说明 | Safety Notice

能量回馈、电力电子和高压实验具有触电、短路、器件爆炸和设备损坏风险。

- 不要将仓库中的规划性内容直接用于市电或高压平台。
- 在没有隔离、限流、急停、过压/过流保护和教师或专业人员监督的情况下，不要开展功率实验。
- 后续代码和硬件资料在完成实机验证前，都应视为实验性内容。

## 当前已有内容 | Available Now

- 项目范围、开发阶段与路线图
- 系统架构和硬件设计说明
- 快速入门、贡献指南与常见问题
- 嵌入式、仿真、硬件和示例目录的规划说明

## 计划实现 | Planned Work

- STM32 固件框架：ADC 采样、PWM、状态机和故障处理
- 双向功率流与能量回馈控制算法
- 过压、过流、过温和短路保护
- MATLAB/Simulink 或其他仿真模型
- 原理图、PCB 与低压验证平台
- 自动化构建、测试记录和可复现实验数据

## 仓库结构 | Repository Structure

```text
ti-cup-energy-feedback-converter/
├── firmware/           # 固件规划；当前为说明文件
├── simulation/         # 仿真规划；当前为说明文件
├── hardware/           # 硬件规划；当前为说明文件
├── examples/           # 示例规划；当前为说明文件
├── docs/
│   ├── ARCHITECTURE.md
│   ├── HARDWARE.md
│   ├── GETTING_STARTED.md
│   ├── ROADMAP.md
│   ├── CONTRIBUTING.md
│   ├── COMPETITION_EXPERIENCE.md
│   └── FAQ.md
├── LICENSE
└── README.md
```

## 从这里开始 | Start Here

1. 阅读 [`docs/ROADMAP.md`](docs/ROADMAP.md)，了解当前阶段和开发顺序。
2. 阅读 [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)，了解系统模块划分。
3. 阅读 [`docs/HARDWARE.md`](docs/HARDWARE.md) 和安全说明，但不要把规划性参数直接用于高压实物。
4. 代码发布后，再按照 [`docs/GETTING_STARTED.md`](docs/GETTING_STARTED.md) 进行编译和测试。

## 贡献 | Contributing

欢迎提交 Issue 或 Pull Request，尤其是：

- 文档纠错和安全性补充
- 控制算法推导、仿真和测试用例
- 可复现的低压实验记录
- 固件结构、硬件接口和保护逻辑改进

提交前请阅读 [`docs/CONTRIBUTING.md`](docs/CONTRIBUTING.md)。

## 许可证 | License

本项目采用 [MIT License](LICENSE)。硬件实验和竞赛使用者仍需自行承担设计验证、规则合规与安全责任。
