# 主机端 Modbus 帧与遥测示例

通用 CRC、请求构造和寄存器响应解析从同账号的 `ti-cup-resources` 提炼，保留固定来源与 MIT 许可证，见 [sources.json](../../tools/sources.json)。

依赖：Python 3.10+，不需要 PySerial 或第三方包。

```bash
python examples/host/converter_telemetry.py request
python tools/modbus_rtu.py read 1 0 12 --input
python -m unittest discover -s tests -v
```

前两个命令输出读取地址 0..11 的功能码 0x04 请求帧，不打开串口或发送字节。

## 解析完整响应

```bash
python examples/host/converter_telemetry.py decode "<完整响应的十六进制字节>"
```

适配 [Modbus 寄存器表](../../docs/MODBUS_REGISTER_MAP.md) 中的 12 个 input registers，输出状态、32 位故障标志、电流 A、母线 V、温度 °C、占空比、参考电流、毫秒计数和版本。

验证 CRC、从站、功能码和寄存器数量；正确解析 int16 负值以及 low/high 16 位组合。拒绝未知状态和超出文档范围的占空比。

这是主机端协议与单位适配。现有 STM32 端口仍需要实现对应的 Modbus 从站映射和真实通信接入；本示例没有对硬件运行、功率级保护或实机连接作验收承诺。

## 已有回归验证

- 通用模块的已知 CRC 向量、构帧、异常响应和越界测试。
- 完整 12 寄存器响应、负值、比例、字序和命令行 JSON。
- CRC 错误、从站错误、功能码错误、长度错误、异常响应、状态/占空比范围。

通用函数应优先在 `ti-cup-resources` 维护；再次同步时更新 `tools/sources.json`，并重跑 Python 与 C 控制核心测试。
