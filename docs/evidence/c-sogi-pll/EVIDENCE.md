# C 证据：SOGI-PLL 锁 U1 + PWM 门控（四件套）

门禁：`/workspace/hardware/evidence-gate-recreate-2026-09-05.md`  
仓：`yniantongtian-oss/ti-cup-energy-feedback-converter`  
分支：`feat/c-sogi-pll`（基于 B）  
日期：2026-09-05

## 1. 计划

**要改什么**

- 新增 portable `sogi_pll`：对植物侧 U1 做 SOGI + Park(q) PI，输出 `theta`/`omega`/`locked`。
- 同步源只有 SOGI-PLL；**禁止 RAMPGEN / 开环斜坡**当角度源。
- `converter_runtime_step` 每拍喂 U1；`pll_gate_pwm` 默认开：未锁则 `duty=0`、`pwm_released=false`。
- 平台注入 ISR：未 `pwm_released` 时强制 CCR=0。
- 仿真 `pll_compare.py`：gated vs ungated（作弊对照）。
- **不做** D–F。

**成功标准**

1. `test_sogi_pll`：正弦 U1 能锁；零 U1 不锁；未锁 duty 恒 0。
2. A/B 单测仍绿。
3. `python3 simulation/pll_compare.py`：锁相时间 <0.15 s；锁前 duty 全 0。
4. 文档写明禁止 RAMPGEN。

## 2. 现场日志

```bash
# 单测（无 cmake）
gcc … sogi_pll.c converter*.c tests/test_sogi_pll.c …
python3 simulation/pll_compare.py
```

完整输出：`/workspace/hardware/evidence/c-sogi-pll/field-log-2026-09-05.txt`  
仓内：`docs/evidence/c-sogi-pll/`

| 产物 | 路径 |
|---|---|
| 日志 | `simulation/pll_compare_log.txt` |
| SVG | `simulation/pll_compare.svg` |
| gated CSV | `simulation/output_pll_gated.csv` |
| ungated CSV | `simulation/output_pll_ungated.csv` |

摘要：

```
gated lock_time_s≈0.0355  final_i≈1.0
pre-lock duty all zero: OK
```

## 3. 失误 / 调整

1. `sogi_pll.c` 首编 `-Werror`：未用变量 `err` → 删掉。
2. 默认 `f0_hz=100`（电赛植物侧常见），不是 50 Hz 市电；若题目是 50 Hz 要改配置，已在默认配置注释/证据标明。
3. 无真板注入波形；锁相证据来自单测 + 主机仿真，不当功率级验证。
4. ungated 曲线仅作反例，产品路径禁止。

## 4. 核对

| 检查项 | 结果 | 证据 |
|---|---|---|
| SOGI-PLL 能锁 100 Hz U1 | 过 | `test_locks_on_sine_u1`；lock≈35 ms |
| 零 U1 不锁 | 过 | `test_no_lock_on_zero_u1` |
| 未锁不放 PWM | 过 | runtime 清 duty；ISR 再关；CSV 锁前 duty=0 |
| 无 RAMPGEN | 过 | 仓内无 ramp/angle generator；文档禁止 |
| A/B 回归 | 过 | converter/runtime/timing 全绿 |
| 未做 D–F | 声明 | 无 Park 电流、无状态机、无 BKIN |

**第三人最小重做**：检出分支 → 跑单测 + `python3 simulation/pll_compare.py` → 对照日志。
