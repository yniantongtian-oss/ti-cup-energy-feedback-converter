# Portable control core | 可移植控制核心

本目录提供一个 **C99、无 MCU 厂商依赖** 的最小控制核心，用于主机端学习、单元测试和后续移植。它不是完整 STM32 工程，也没有经过真实功率平台验证。

## 已实现

- `IDLE / RUN / FAULT` 状态机
- 上电和未解锁时强制零输出
- 双向电流参考值与斜率限制
- PI 电流环输出电压命令 `u_i`，占空比 `(u_i + U1) / Vbus`，前馈默认开
- 母线过低拒绝调制（不除零）
- 过流、母线过压/欠压、过温、无效采样和异常步长锁存故障
- 仅在停机且测量恢复安全后清除故障
- 主机演示程序与单元测试

## 构建与测试

```bash
cmake -S firmware -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/converter_demo
```

Windows 使用 Visual Studio 生成器时，演示程序通常位于 `build/Release/`。

## 移植到 STM32 的建议接口

1. **电流环 ~20 kHz**：注入组采电流+U1，在注入完成 ISR 里调用 `converter_step(..., dt=1/20000)` 并写 PWM。
2. **1 kHz**：只更新母线/温度、Modbus、安全；不要把电流环绑在 1 ms。
3. 仅在硬件急停、驱动故障输入和软件状态均允许时，把 `duty_command` 映射到 PWM。
4. `duty_command` 是 `[-1, 1]` 范围内的有符号归一化命令，由 `(u_i + U1)/Vbus` 得到（前馈可关，默认开）；方向和桥臂映射必须由硬件适配层完成。U1 来自植物侧电压采样。
5. 硬件比较器、栅极驱动器故障和独立关断链路不能由本软件替代。触发用 TIM1 OCxREF，不用 Update。
6. **SOGI-PLL 锁 U1**：未锁时 `duty` 强制 0，禁止 RAMPGEN 当同步源。

## 安全边界

默认参数仅用于测试代码行为，不是经过验证的功率级参数。首次移植只能在隔离、限流、低压电源和可快速断电的条件下进行。
