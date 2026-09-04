# F 证据：PA6 TIM1_BKIN（四件套）

门禁：`/workspace/hardware/evidence-gate-recreate-2026-09-05.md`  
分支：`feat/f-tim1-bkin`（基于 E）  
日期：2026-09-05

## 1. 计划

**要改什么**

- PA6 = TIM1_BKIN，低有效（BKP=0），BKE=1，**AOE=0**。
- 硬件 Break 先清 **MOE**，软件 ISR 只锁存；清故障须 PLL 再锁后**显式**置 MOE。
- PB12 仍为 GPIO 故障，**不替代** BKIN。
- 比较器未上板也按 PA6 写固件；不改板、不下单。
- 可移植 `tim1_bkin` 合同 + 平台 `app_tim1_bkin.c`。

**成功标准**

1. `test_tim1_bkin`：BDTR 位合同、break 清 MOE、AOE=0 路径绿。
2. `python3 simulation/bkin_compare.py`：断点时刻 MOE 先于软清；AOE=0 时软清后 MOE 仍 0。
3. A–E 回归绿；文档钉 PA6/PB12。

## 2. 现场日志

```bash
# 单测含 firmware/tests/test_tim1_bkin.c
python3 simulation/bkin_compare.py
```

完整：`/workspace/hardware/evidence/f-tim1-bkin/field-log-2026-09-05.txt`  
仓内：`docs/evidence/f-tim1-bkin/`

| 产物 | 路径 |
|---|---|
| 日志 | `simulation/bkin_compare_log.txt` |
| SVG | `simulation/bkin_compare.svg` |
| CSV | `simulation/output_bkin.csv` |

## 3. 失误 / 调整

1. 仓内无完整 Cube `main.h`：平台 `app_tim1_bkin.c` 依赖 HAL 符号，主机只测 portable `tim1_bkin.c`；平台文件以合同+注释钩子交付。
2. 曾考虑用 PB12 兼 BKIN——按 RM0008 F103 默认 BKIN=PA6，**拒绝**；证据写明。
3. 软件清 trip 不能冒充比较器已接；README 写「未上板也按 PA6 写」。

## 4. 核对

| 检查项 | 结果 | 证据 |
|---|---|---|
| BKE=1 BKP=0 AOE=0 | 过 | `tim1_bkin_bdtr_bits` / README |
| Break 清 MOE | 过 | test + sim t=0.050 |
| AOE=0 无自动恢复 | 过 | sim t=0.120 MOE 仍 0 |
| PB12 非 BKIN | 过 | 文档 + 代码注释 |
| A–E 回归 | 过 | field-log |
| 未改板/下单 | 声明 | 仅固件/文档 |

**第三人最小重做**：检出分支 → `test_tim1_bkin` + `python3 simulation/bkin_compare.py`。
