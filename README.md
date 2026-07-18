# ti-cup-energy-feedback-converter

**English**: Open-source Energy Feedback Converter Toolkit for National Undergraduate Electronic Design Competition (TI Cup)  
**中文**: TI Cup 能量回馈变流器负载试验装置开源工具包（A题）

---

## 项目简介 | Project Overview

本项目为 **2026 年全国大学生电子设计竞赛（TI Cup）福建赛区 A 题** －— 《能量回馈的变流器负载试验装置》提供开源参考实现与工具包。

项目包含基于 STM32 的固件、双向功率控制算法、能量回馈策略、保护机制、仿真模型以及硬件集成示例，旨在帮助大学生参赛队伍快速实现高性能、安全可靠的能量回馈变流器系统，同时为光电信息科学与工程、电气工程、自动化等专业学生提供实用学习资源。

This project provides an open-source reference implementation and toolkit for **Topic A of the 2026 National Undergraduate Electronic Design Competition (TI Cup)** — Energy Feedback Converter Load Test Apparatus.

It includes STM32-based firmware, bidirectional power flow control algorithms, energy recovery strategies, protection mechanisms, simulation models, and hardware integration examples. The goal is to help undergraduate teams quickly develop high-performance and safe energy feedback converter systems, while serving as a practical learning resource for students in Optoelectronics, Electrical Engineering, and Automation majors.

---

## 功能特性 | Key Features

- STM32 固件框架与实时控制代码（支持多种工作模式）
- 双向功率流控制与能量回馈算法
- 多重保护机制（过压、过流、过温、短路等）
- 仿真模型与参数调优支持
- 硬件原理图与 PCB 设计参考（Altium / 立创 EDA）
- 竞赛评分导向的优化策略与测试方法
- 中英双语文档与教程（持续完善中）

---

## 技术栈 | Tech Stack

- **MCU**: STM32F103 / STM32F407 等主流型号
- **开发环境**: Keil MDK / STM32CubeIDE
- **硬件设计**: Altium Designer / 立创 EDA
- **仿真与建模**: MATLAB / Simulink / PSIM（计划支持）
- **通信**: CAN / RS485 / Modbus（可选扩展）

---

## 目录结构 | Directory Structure

```
ti-cup-energy-feedback-converter/
├── firmware/              # STM32 固件源码
│   ├── Core/
│   ├── Drivers/
│   └── User/              # 控制算法与应用层
├── simulation/            # 仿真模型与脚本
├── hardware/              # 原理图与 PCB（立创/Altium）
├── docs/                  # 文档与教程
│   ├── zh/                # 中文文档
│   └── en/                # English documentation
├── examples/              # 竞赛场景示例工程
└── README.md
```

> **当前状态**：仓库刚创建，代码与文档正在逐步完善中。欢迎 Star 和关注，后续会持续更新固件、算法和完整文档。

---

## 快速开始 | Quick Start

（待代码 push 后补充详细步骤，目前可参考后续更新）

1. 克隆仓库
2. 使用 Keil / STM32CubeIDE 打开 firmware 工程
3. 根据硬件原理图配置引脚与外设
4. 编译下载并进行功能测试

详细步骤将在 `docs` 目录中逐步补充。

---

## 硬件要求 | Hardware Requirements（参考）

- 主控：STM32F1 / F4 系列开发板
- 功率级：双向 Buck-Boost 或类似拓扑
- 传感器：电压、电流、温度采样电路
- 负载：可编程电子负载或实际负载
- 示波器、万用表等调试设备

---

## 贡献指南 | Contributing

本项目欢迎大学生参赛者、老师以及嵌入式/电力电子爱好者参与贡献！

贡献方式包括但不限于：
- 提交 Issue（Bug 反馈、功能建议）
- 提交 Pull Request（算法优化、文档完善、新示例）
- 分享实际比赛使用经验

贡献前请先阅读 `docs/CONTRIBUTING.md`（即将添加）。

---

## 许可证 | License

本项目采用 [MIT License](LICENSE)。

---

## 相关链接 | Related Links

- 全国大学生电子设计竞赛官网
- TI 官方参考资源
- 本项目配套 Claude for Open Source 申请说明

---

## 联系与反馈 | Contact & Feedback

如有任何问题、建议或合作意向，欢迎通过 GitHub Issues 联系。

**维护者**：yniantongtian-oss（龙岩学院 光电信息科学与工程 25级学生）

---

**Star 本项目**，一起为 TI Cup 参赛队伍提供更好的开源资源！