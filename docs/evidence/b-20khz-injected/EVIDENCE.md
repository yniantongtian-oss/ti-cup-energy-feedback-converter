# B 证据：TIM1 中心对齐 ~20 kHz 注入采样（四件套）

门禁：`/workspace/hardware/evidence-gate-recreate-2026-09-05.md`  
仓：`yniantongtian-oss/ti-cup-energy-feedback-converter`  
分支：`feat/b-20khz-injected-sampling`（基于 A）  
日期：2026-09-05

## 1. 计划

**要改什么**

- 电流环从 `AppConverter_1msTask`（dt=1 ms）迁出。
- ~20 kHz：TIM1 中心对齐；ADC **注入组**采电流(IN0)+U1(IN3)；触发 = TIM1 **OCxREF**（禁止 Update）。
- 注入完成回调：`AppConverter_CurrentLoopFromInjected` → `converter_step(..., dt=1/20000)` → 写 PWM。
- 1 kHz：规则组 DMA 只刷母线/温度缓存 + Modbus/安全；**不跑电流 PI、不写占空比**。
- 仿真：`--rate-compare` 对比控制率 20 kHz vs 1 kHz（前馈开，U1 仅在控制节拍采样）。
- **不做** C–F（SOGI-PLL / Park / 状态机 / BKIN）。PA6 仍只预留。

**成功标准**

1. 单元测试含 20 kHz dt 长跑绿；旧 A 测试仍绿。
2. `python3 simulation/simulate.py --rate-compare`：1 kHz RMS 误差 > 1.5× 的 20 kHz。
3. 平台文档写清 CubeMX：注入 vs 规则、OCxREF、1 ms 职责。
4. 第三人能按命令重做。

## 2. 现场日志

```bash
# 单元测试（无 cmake 时）
gcc … firmware/tests/test_timing_rate.c …  # 见 field-log
/tmp/conv-build-b/test_converter && /tmp/conv-build-b/test_runtime && /tmp/conv-build-b/test_timing

python3 simulation/simulate.py --rate-compare
```

完整输出：`/workspace/hardware/evidence/b-20khz-injected/field-log-2026-09-05.txt`  
仓内副本：`docs/evidence/b-20khz-injected/`

| 产物 | 路径 |
|---|---|
| 对比日志 | `simulation/rate_compare_log.txt` |
| 波形 SVG | `simulation/rate_compare.svg` |
| 20 kHz CSV | `simulation/output_rate_20khz.csv` |
| 1 kHz CSV | `simulation/output_rate_1khz.csv` |

摘要（本次）：

```
20 kHz Irms_err≈0.000001 A  updates=8000
1 kHz  Irms_err≈0.567273 A  updates=401
1k/20k rms ratio ≫ 1.5
```

关键代码：

- `platforms/.../app_converter.c`：`AppConverter_CurrentLoopFromInjected` / `AppConverter_1msTask`
- `platforms/.../README.md`：CubeMX 注入 + OCxREF
- `firmware/tests/test_timing_rate.c`

## 3. 失误 / 调整

1. 首版 `test_timing_rate` 只跑 200 拍且默认 slew=5 A/s → 10 ms 内参考只到 0.05 A，断言失败。改道：slew=50、跑 0.4 s（8000 拍）。
2. 证据门禁要求失败也记：本条保留。
3. 仍无真板 / 无 Cube 工程二进制；B 的硬件侧是端口代码+文档合同，仿真证控制率，不当功率级验证。
4. 注入完成回调以注释形式钉在 `app_converter.c` 末尾（仓内无完整 `stm32f1xx_it.c`），避免假装已有整棵 Cube 树。

## 4. 核对

| 检查项 | 结果 | 证据 |
|---|---|---|
| 电流环 dt=50 µs API | 过 | `APP_CURRENT_LOOP_DT_S`；`test_timing_rate` |
| 1 ms 不再跑 PI/PWM | 过 | `AppConverter_1msTask` 只写慢 ADC 缓存 |
| 注入 vs 规则分离 | 过 | 平台 README + `g_adc_dma[2]` 仅 bus/temp |
| 触发禁止 Update | 过 | README / 文件尾注释写死 OCxREF |
| 20 kHz 优于 1 kHz | 过 | `rate_compare_log.txt` |
| 未做 C–F | 声明 | 无 SOGI/Park/状态机/BDTR |

**第三人最小重做**：检出本分支 → 跑上面命令 → 对照 `rate_compare_log.txt` 与 SVG。
