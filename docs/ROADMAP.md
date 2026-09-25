# Project Roadmap | 项目路线图

This document outlines the current status and future development plans for **ti-cup-energy-feedback-converter**.

## Current Status (September 2026) | 当前状态

- Portable C99 control/runtime core is implemented and CI-tested on Windows, macOS and Ubuntu.
- A STM32F103C8T6 reference adapter, ADC/DMA integration layer and Modbus register map are present.
- Host-side simulation and software fault/watchdog/calibration logic have automated coverage.
- The repository does **not** yet contain a manufacturable power-stage design or traceable real-hardware validation evidence.
- Hardware validation evidence must satisfy `docs/RELEASE_ACCEPTANCE.md` before a hardware-qualified release.

## Short-term Goals (Next 1-2 months) | 短期目标

- [ ] Complete basic STM32 firmware skeleton (control loop, ADC sampling, PWM generation)
- [ ] Implement core bidirectional power flow control algorithm
- [ ] Add protection mechanisms (over-voltage, over-current, over-temperature)
- [ ] Create initial simulation model (MATLAB/Simulink)
- [ ] Write bilingual getting-started documentation
- [ ] Add hardware reference schematic (basic version)

## Medium-term Goals (3-6 months) | 中期目标

- [ ] Full competition-ready firmware with multiple operating modes
- [ ] Comprehensive test cases and validation on real hardware
- [ ] Detailed tutorials and example projects for different competition scenarios
- [ ] CI/CD pipeline for automated building and testing
- [ ] Community feedback collection from other TI Cup teams

## Long-term Vision | 长期愿景

- Become a useful open-source reference for Chinese undergraduate power electronics and embedded systems education
- Support multiple TI Cup / similar competition topics
- Build a small active contributor community
- Continuously improve code quality and documentation with the help of advanced AI tools (Claude)

## How Claude Will Help | Claude 如何帮助

Having access to Claude will significantly accelerate:

- Rapid prototyping of complex control algorithms
- High-quality bilingual documentation and tutorials
- Code refactoring and architecture improvements
- Generating test cases and simulation scripts
- Maintaining momentum while balancing studies and competition preparation

This project is expected to be actively maintained throughout my undergraduate studies and beyond.

---

**Last Updated**: September 2026