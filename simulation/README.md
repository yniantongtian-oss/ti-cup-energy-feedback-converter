# Simulation | 仿真

Averaged host model for steps **A** (feedforward) and **B** (control rate).

```bash
# A: FF ON vs OFF @ 20 kHz control
python3 simulation/simulate.py --compare

# B: 20 kHz vs 1 kHz control (FF ON, U1 held between updates)
python3 simulation/simulate.py --rate-compare
```

B writes `output_rate_20khz.csv`, `output_rate_1khz.csv`, `rate_compare.svg`, `rate_compare_log.txt`.

Not included: SOGI-PLL, Park, state machine, BKIN (C–F).

```bash
# C: SOGI-PLL lock gate
python3 simulation/pll_compare.py
```

```bash
# D: Park + Iq=0
python3 simulation/park_compare.py
```

```bash
# E: bring-up SM
python3 simulation/bringup_compare.py
```
