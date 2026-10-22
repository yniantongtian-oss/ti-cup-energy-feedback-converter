# Simulation | 仿真

Host-side digital twin of the portable current loop and single-phase SOGI-PLL. It is an averaged model for learning and gain trials. It is not a switching model, and it does not contain measured magnetics, efficiency, or thermal data.

## Run

```bash
python3 simulation/simulate.py --duration 0.5 --output simulation/output.csv
python3 simulation/tune.py suggest --require-stable --json
python3 simulation/tune.py pll --frequency 49 --json
python3 simulation/tune.py schema
```

`tune.py` writes CSV and SVG artifacts under `simulation/runs/latest/` unless `--output-dir` is set. Stdout is one JSON object. `--human` prints a short summary instead. The CLI does not flash firmware or arm hardware.

The current-loop plant is `di/dt = (K * duty - current) / tau` with the same default `K = 2` and `tau = 20 ms` as the host demo. Protection thresholds match the C core, including trips at the threshold itself.

## Tests

```bash
python3 -m pip install -r requirements-dev.txt
python3 -m pytest tests/test_digital_twin.py tests/test_tune_cli.py
```

MATLAB, Simulink, and PSIM models are still absent.
