# Getting Started | 快速开始

This guide is the host-side path through the portable control core. The STM32F103 reference port is a separate integration step and is not a Keil project inside `firmware/`.

## Prerequisites | 前置要求

- CMake 3.16 or newer and a C99 compiler
- Python 3.10 or newer
- For board work later: an STM32F103 board, STM32CubeIDE or Keil, and an isolated current-limited supply

## Host quick start | 主机快速开始

```bash
git clone https://github.com/yniantongtian-oss/ti-cup-energy-feedback-converter.git
cd ti-cup-energy-feedback-converter
cmake -S firmware -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
python3 -m pip install -r requirements-dev.txt
python3 -m pytest
python3 simulation/tune.py run --duration 0.2 --json
```

The tuning command writes `simulation/runs/latest/current_loop.csv` and an SVG plot. Open the SVG in a browser. The JSON on stdout reports settling, overshoot, steady-state error, and fault bits.

## STM32 reference port | STM32 参考端口

`platforms/stm32f103-bluepill/` shows how to call the portable core from STM32Cube HAL. Create the CubeMX project yourself, add the firmware sources listed in that directory's README, and keep the first experiment on an isolated low-voltage supply. Do not open `firmware/` as a Keil power-stage project; that directory is the host-testable C99 library.

## Next steps | 下一步

- Read `docs/ROADMAP.md` for what is still unvalidated.
- Read `docs/HARDWARE.md` and `docs/MODBUS_REGISTER_MAP.md` before wiring anything.
- Use `examples/host/` for Modbus frame decoding. It does not open a serial port.

> Software protection and the digital twin do not replace fuses, isolation, hardware comparators, or a qualified review.
