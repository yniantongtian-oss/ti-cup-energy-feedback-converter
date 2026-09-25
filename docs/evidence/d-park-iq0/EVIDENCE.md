# D 证据：Park + Iq=0（四件套）

门禁：`/workspace/hardware/evidence-gate-recreate-2026-09-05.md`  
仓：`yniantongtian-oss/ti-cup-energy-feedback-converter`  
分支：`feat/d-park-iq0`（基于 C）  
日期：2026-09-05

## 1. 计划

**要改什么**

- 电流环改为同步坐标系：**Park**，`Id_ref` = 电流给定（斜率后），**`Iq_ref = 0`**。
- 内环仍是 **双 PI**（Id/Iq）；**不堆** 3/5/7 谐波 PR。
- `i_beta` 由电流 SOGI（跟 PLL ω）产生；`theta` 来自已锁 SOGI-PLL。
- 反 Park → `u_alpha`，再 `(u_alpha+U1)/Vbus`（A 前馈保留）。
- 无 theta 时回退静止系 PI（单测/未锁），仍无 PR。
- **不做** E–F。

**成功标准**

1. `test_park_iq0` 绿；A–C 回归绿。
2. `python3 simulation/park_compare.py`：Park ON 的 Iq RMS 明显小于静止系 PI（ratio>1.5）。
3. 文档写明 Iq=0、不堆 PR。

## 2. 现场日志

```bash
# 单测含 firmware/tests/test_park_iq0.c
python3 simulation/park_compare.py
```

完整输出：`/workspace/hardware/evidence/d-park-iq0/field-log-2026-09-05.txt`  
仓内：`docs/evidence/d-park-iq0/`

| 产物 | 路径 |
|---|---|
| 日志 | `simulation/park_compare_log.txt` |
| SVG | `simulation/park_compare.svg` |
| Park ON CSV | `simulation/output_park_on.csv` |
| Park OFF CSV | `simulation/output_park_off.csv` |

摘要：

```
Park ON  Iq_rms≈0.0097 A
Park OFF Iq_rms≈1.2247 A
OFF/ON ratio≈126
```

## 3. 失误 / 调整

1. 测量结构加 `current_beta_a` / `theta_*` 后，旧单测初始化缺字段 → 补齐，否则 C99 未初始化读脏值。
2. 主机仿真用理想角隔离 Park 效果；固件路径用 PLL theta（未锁不放 PWM 仍由 C 门控）。
3. 不当功率级验证；无谐波 PR 状态机。

## 4. 核对

| 检查项 | 结果 | 证据 |
|---|---|---|
| Park 默认开 | 过 | `park_enable=true` |
| Iq_ref=0 | 过 | `converter.c` 写死 `iq_ref = 0` |
| 双 PI 无 PR | 过 | 仅 `integrator_d/q` |
| Iq RMS 下降 | 过 | `park_compare_log.txt` |
| 回归 A–C | 过 | field-log 全绿 |
| 未做 E–F | 声明 | 无状态机/BKIN |

**第三人最小重做**：检出分支 → 单测 + `python3 simulation/park_compare.py`。
