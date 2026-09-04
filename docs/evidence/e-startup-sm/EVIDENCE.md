# E 证据：上电状态机 + 母线慢抬（四件套）

门禁：`/workspace/hardware/evidence-gate-recreate-2026-09-05.md`  
分支：`feat/e-startup-sm`（基于 D）  
日期：2026-09-05

## 1. 计划

**要改什么**

- 状态机：`OPEN_LOOP` → `CURRENT_DC`（直流 Id）→ `OUTER_V`（外电压给 Id_ref）。
- 开环用小占空比核对调制/采样；切电流环前需 PLL 已锁。
- `bus_cmd` 从 `bus_start_v` 按 `bus_ramp_v_per_s` 慢抬到 `bus_target_v`。
- 1 kHz：`converter_runtime_bringup_1khz`；20 kHz 仍跑电流环/开环占空比。
- **不做 F**（BKIN 仍预留）。

**成功标准**

1. `test_bringup`：模式顺序与母线慢抬断言绿；A–D 回归绿。
2. `python3 simulation/bringup_compare.py`：modes_seen 含三段；终态 `bus_cmd=target`。
3. 文档写清顺序。

## 2. 现场日志

```bash
# 单测含 firmware/tests/test_bringup.c
python3 simulation/bringup_compare.py
```

完整：`/workspace/hardware/evidence/e-startup-sm/field-log-2026-09-05.txt`  
仓内：`docs/evidence/e-startup-sm/`

| 产物 | 路径 |
|---|---|
| 日志 | `simulation/bringup_compare_log.txt` |
| SVG | `simulation/bringup_compare.svg` |
| CSV | `simulation/output_bringup.csv` |

摘要：`modes_seen=['OPEN_LOOP','CURRENT_DC','OUTER_V']`，`final bus_cmd=24`。

## 3. 失误 / 调整

1. 默认 `bringup_enable=true` 后，A–D 层测若只调 20 kHz、不调 1 kHz，会一直 `current_enable=false` 把 duty 清零。改道：A–D 单测显式 `bringup_enable=false`；产品路径 App 1 ms 调 bringup。
2. `bus_cmd` 是软件慢抬命令，不是电源本体；证据写明须配合限流电源。
3. 外环用 PLL `amplitude_v` 当 v_out 测量（演示）；真板可换独立电压反馈。

## 4. 核对

| 检查项 | 结果 | 证据 |
|---|---|---|
| 模式顺序 | 过 | test_mode_sequence / bringup_compare_log |
| 先直流电流环 | 过 | CURRENT_DC 的 id_ref=id_dc_a |
| 再外电压 | 过 | OUTER_V outer_enable |
| 母线慢抬 | 过 | 1 s 约 +ramp；终到 target |
| PLL 门控仍在 | 过 | 未锁 duty 仍 0（C） |
| 未做 F | 声明 | 无 BDTR/BKIN |

**第三人最小重做**：检出分支 → `test_bringup` + `python3 simulation/bringup_compare.py`。
