# ti-cup-energy-feedback-converter

**Open-source Toolkit for Energy Feedback Converter Load Test Apparatus**  
**TI Cup 2026 Topic A | National Undergraduate Electronic Design Competition**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)  
![Status](https://img.shields.io/badge/Status-Active%20Development-orange)  
![Target](https://img.shields.io/badge/Target-TI%20Cup%20Teams-blue)

---

## 项目简介 | Project Overview

本项目旨在为 **2026 年全国大学生电子设计竞赛（TI Cup）A 题** ——《能量回馈的变流器负载试验装置》提供高质量的开源参考实现与工具包。

我们希望通过系统化的固件框架、控制算法、保护机制和文档体系，帮助大学生参赛队伍在有限时间内快速构建高性能、安全可靠的能量回馈变流器系统，同时为光电信息科学与工程、电气工程、自动化等专业的学生提供实用且可扩展的学习资源。

This project provides a high-quality open-source reference implementation and toolkit for **Topic A of the 2026 National Undergraduate Electronic Design Competition (TI Cup)** — Energy Feedback Converter Load Test Apparatus.

Our goal is to help undergraduate teams rapidly develop high-performance, safe, and reliable energy feedback converter systems within tight competition timelines, while offering practical and extensible learning resources for students in Optoelectronics, Electrical Engineering, and Automation.

---

## 为什么选择这个项目？ | Why This Project?

- **实战导向**：基于真实 TI Cup 备赛经验，总结了算法与硬件调试、保护机制、能量回馈效率优化等常见问题与解决方案。
- **学生友好**：提供清晰的架构说明、双语文档和逐步指南，降低入门门槛。
- **可扩展性**：模块化设计，方便后续扩展新功能和新硬件平台。
- **社区价值**：鼓励分享比赛经验和改进，共同提升中国大学生电子设计竞赛的整体水平。

---

## 功能特性 | Key Features

- 基于 STM32 的模块化固件框架，支持多种工作模式
- 双向功率流控制与能量回馈核心算法
- 多层保护机制（过压、过流、过温、短路等）
- 仿真模型与参数优化支持
- 硬件原理图与 PCB 设计参考（Altium / 立创 EDA）
- 竞赛评分导向的测试方法与优化策略
- 完整的中英双语文档与教程体系

---

## 目标用户 | Target Audience

- 正在准备 TI Cup 及其他电子设计竞赛的大学生团队
- 希望学习功率电子与嵌入式控制的在校学生
- 对电力电子、STM32 开发感兴趣的爱好者与教师

---

## 当前状态 | Current Status

**开发阶段**：早期开发中（Active Development）

已完成：
- 项目整体架构与文档体系
- 真实比赛经验总结与实用建议
- 贡献指南、路线图、上手文档等

进行中：
- 核心固件框架与控制算法实现
- 硬件参考设计
- 仿真模型搭建

---

## 目录结构 | Directory Structure

```
ti-cup-energy-feedback-converter/
├── firmware/           # STM32 固件（开发中）
├── simulation/         # 仿真模型与脚本（规划中）
├── hardware/           # 原理图与 PCB 设计（规划中）
├── examples/           # 竞赛场景示例（规划中）
├── docs/               # 完整文档体系
│   ├── COMPETITION_EXPERIENCE.md   # 真实比赛经验
│   ├── ROADMAP.md                  # 项目路线图
│   ├── ARCHITECTURE.md             # 系统架构
│   ├── HARDWARE.md                 # 硬件参考
│   ├── GETTING_STARTED.md          # 上手指南
│   ├── CONTRIBUTING.md             # 贡献指南
│   └── FAQ.md                      # 常见问题
└── README.md
```

---

## 快速开始 | Quick Start

详细的上手步骤请参考 [`docs/GETTING_STARTED.md`](docs/GETTING_STARTED.md)。

基本流程：
1. 克隆本仓库
2. 使用 Keil MDK 或 STM32CubeIDE 打开 firmware 工程
3. 根据 `docs/HARDWARE.md` 配置硬件引脚
4. 编译、下载并进行基础功能测试

---

## 贡献 | Contributing

我们非常欢迎大学生参赛者、教师以及嵌入式/电力电子爱好者参与贡献！

贡献方式包括但不限于：
- 提交 Issue（问题反馈、功能建议、经验分享）
- 提交 Pull Request（代码、文档、示例）
- 分享真实比赛使用反馈

请先阅读 [`docs/CONTRIBUTING.md`](docs/CONTRIBUTING.md)。

---

## 许可证 | License

本项目采用 [MIT License](LICENSE)。

---

## 联系我们 | Contact

如有任何问题、建议或合作意向，欢迎通过 **GitHub Issues** 联系我们。

**维护者**：yniantongtian-oss（龙岩学院 · 光电信息科学与工程 · 25级）

---

**如果这个项目对你有帮助，欢迎 Star ⭐ 支持我们！**

我们希望通过这个开源项目，帮助更多 TI Cup 参赛队伍取得更好成绩，也为中国大学生电子设计教育贡献一份力量。