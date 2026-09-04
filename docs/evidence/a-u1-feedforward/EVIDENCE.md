# A 证据：U1 前馈占空比（四件套）

门禁：`/workspace/hardware/evidence-gate-recreate-2026-09-05.md`  
仓：`yniantongtian-oss/ti-cup-energy-feedback-converter`  
工作树（本机）：`/workspace/audit-tiday6e`  
日期：2026-09-05（UTC+8；现场日志时间戳为 UTC）

## 1. 计划

**要改什么**

- 测量加植物侧 `U1`（软件通道 PA3 / ADC1_IN3；板上可暂无探头）。
- 电流环 PI 输出改为电压命令 `u_i`（不再直接当占空比）。
- 占空比公式（默认开前馈）：`duty = (u_i + U1) / Vbus`；关前馈时 `duty = u_i / Vbus`。
- `Vbus ≤ mod_bus_min_v`（默认 1 V）时强制 `duty = 0`，不除零。
- 仿真加 100 Hz `U1` 扰动，对比 FF ON / OFF。
- **不做** B–F（20 kHz 注入、SOGI-PLL、Park、状态机、TIM1 BKIN）。不抄 C2000，不加 PR/RAMPGEN。

**成功标准**

1. 单元测试：前馈默认开；ON 时 duty 含 U1；低母线 duty=0；旧 arm/fault/slew 仍过。
2. `python3 simulation/simulate.py --compare`：FF ON 的电流 RMS 误差明显小于 OFF（门限：OFF/ON > 1.5）。
3. 产物路径写死，第三人能按下面命令重做。

## 2. 现场日志

复现命令（在 `/workspace/audit-tiday6e`）：

```bash
# 单元测试（无 cmake 时用 gcc）
mkdir -p /tmp/conv-build
gcc -std=c99 -Wall -Wextra -Wpedantic -Werror -Ifirmware/include \
  -c firmware/src/converter.c -o /tmp/conv-build/converter.o
gcc -std=c99 -Wall -Wextra -Wpedantic -Werror -Ifirmware/include \
  -c firmware/src/converter_runtime.c -o /tmp/conv-build/runtime.o
gcc -std=c99 -Wall -Wextra -Wpedantic -Werror -Ifirmware/include \
  firmware/tests/test_converter.c /tmp/conv-build/converter.o -lm \
  -o /tmp/conv-build/test_converter
gcc -std=c99 -Wall -Wextra -Wpedantic -Werror -Ifirmware/include \
  firmware/tests/test_runtime.c /tmp/conv-build/converter.o /tmp/conv-build/runtime.o -lm \
  -o /tmp/conv-build/test_runtime
/tmp/conv-build/test_converter
/tmp/conv-build/test_runtime

# 仿真对比
python3 simulation/simulate.py --compare
```

仿真参数（默认）：`Vbus=24 V`，`U1=8 Vpk @ 100 Hz`，`Iref=1 A`，`dt=0.0002 s`，`duration=0.4 s`。

本次完整现场输出：  
`/workspace/hardware/evidence/a-u1-feedforward/field-log-2026-09-05.txt`

仿真产物（同目录副本 + 工作树）：

| 文件 | 路径 |
|---|---|
| FF ON CSV | `/workspace/audit-tiday6e/simulation/output_ff_on.csv`（副本 `.../evidence/a-u1-feedforward/output_ff_on.csv`） |
| FF OFF CSV | `/workspace/audit-tiday6e/simulation/output_ff_off.csv` |
| 对比 SVG | `/workspace/audit-tiday6e/simulation/ff_compare.svg` |
| 对比日志 | `/workspace/audit-tiday6e/simulation/ff_compare_log.txt` |

`ff_compare_log.txt` 摘要：

```
FF ON  Irms_err=0.000001 A  final=1.0000 A
FF OFF Irms_err=2.163095 A  final=1.7282 A
OFF/ON rms ratio≈2e6
```

关键代码：

- `firmware/src/converter.c`：`duty = (u_i + u1) / vbus`
- `platforms/stm32f103-bluepill/Core/Src/app_converter.c`：`g_adc_dma[4]`，`plant_adc = g_adc_dma[3]`（PA3）

## 3. 失误 / 调整

1. **Cloud Agent 开不了**（plan 无 Cloud Agents）。改道：在本机 `/workspace/audit-tiday6e` 改代码，不克隆整仓。
2. **本机无 cmake**：`which cmake` 空。改道：直接 `gcc` 编测试；成功标准仍按测试二进制绿。
3. **无 matplotlib**：不能出 PNG。改道：`simulate.py` 自写 SVG（`ff_compare.svg`）。
4. **推 GitHub blob 422**：2026-08-31 用 Contents/git blobs UTF-8 一次性推失败（Validation Failed）。当时未完成 PR。本次改道：分文件 base64 推，或浅克隆 push；大 CSV 若仍 422 则仓内只留 log+svg+可复现脚本，CSV 留证据目录。
5. **kp/ki 旧值按「duty」量纲**：改成 `u_i`（伏）后默认 `kp=2`、`ki=200`，仍是演示值，不是 20 kHz 调参。注明，避免当已调环。
6. **仿真 ON 误差近 0**：平均模型 + 理想前馈抵消，理想化；只证明公式方向，不能当功率级验证。

## 4. 核对

| 检查项 | 结果 | 证据 |
|---|---|---|
| 前馈默认开 | 过 | `converter_default_config().feedforward_enable == true`；`test_safe_startup` / `test_default_calibration` |
| ON：duty=(u_i+U1)/Vbus | 过 | `test_feedforward_duty_formula`：U1=5、Vbus=10、u_i=1 → duty=0.6 |
| OFF：duty=u_i/Vbus | 过 | 同测试 → duty=0.1 |
| 低母线 duty=0 | 过 | `test_low_bus_forces_zero_duty` |
| 单元测试全绿 | 过 | `field-log-2026-09-05.txt` |
| FF ON 优于 OFF | 过 | `ff_compare_log.txt`：OFF RMS ≫ ON |
| 波形可打开 | 过 | `ff_compare.svg`（绿=ON，红=OFF，灰=ref） |
| 未做 B–F | 声明 | 无 SOGI/PLL/Park/BKIN/20 kHz ISR；F 脚 PA6 只写进平台 README 预留 |

**第三人最小重做**：clone/检出含 A 的分支 → 跑上面两段命令 → 对照 `ff_compare_log.txt` 数量级（ON≪OFF）与 SVG。

## 未完成（不假装过）

- 代码尚未成功合入 `main`（blob 推送曾失败；本证据包先交，推仓并行）。
- 板无 U1 探头、无比较器；A 只接软件通道与仿真。
- F（PA6 BKIN）未配 BDTR。
