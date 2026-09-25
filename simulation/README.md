# Simulation | 仿真

Averaged current-loop host model for **step A** (U1 feedforward). Not a switching model.

```bash
python3 simulation/simulate.py --compare
```

Writes:

- `simulation/output_ff_on.csv` / `output_ff_off.csv`
- `simulation/ff_compare.svg`
- `simulation/ff_compare_log.txt`

Plant: `L di/dt = duty*Vbus - U1 - R*i` with a 100 Hz U1 disturbance.
ON uses `duty = (u_i + U1)/Vbus`; OFF uses `duty = u_i/Vbus`.

B–F (20 kHz ISR, SOGI-PLL, Park, state machine, BKIN) are not in this sim.
