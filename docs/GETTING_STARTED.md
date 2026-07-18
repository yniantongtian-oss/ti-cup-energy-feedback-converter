# Getting Started | 快速开始

This guide will help you quickly understand and start using the **ti-cup-energy-feedback-converter** project.

## Prerequisites | 前置要求

- STM32 development board (F103 / F407 recommended)
- Keil MDK or STM32CubeIDE
- Basic knowledge of power electronics and embedded systems
- Oscilloscope and multimeter for hardware testing

## Quick Start Steps | 快速开始步骤

1. **Clone the repository**
   ```bash
   git clone https://github.com/yniantongtian-oss/ti-cup-energy-feedback-converter.git
   cd ti-cup-energy-feedback-converter
   ```

2. **Open the firmware project**
   - Open `firmware/` folder with Keil MDK or STM32CubeIDE

3. **Configure hardware**
   - Refer to `hardware/` directory for schematic reference
   - Update pin definitions according to your actual board

4. **Build and flash**
   - Compile the project
   - Download to your STM32 board

5. **Test basic functions**
   - Check PWM output
   - Verify ADC sampling
   - Test protection functions

## Next Steps | 下一步

- Read `docs/ROADMAP.md` to understand development plans
- Check `docs/HARDWARE.md` for hardware details
- Explore `examples/` folder for competition scenarios

> **Note**: This project is still under active development. More complete code and tutorials will be added progressively.