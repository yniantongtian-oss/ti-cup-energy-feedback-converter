#!/usr/bin/env python3
"""Dependency-free averaged current-loop demonstration.

Educational host simulation only; it is not a switching model and does not
contain validated hardware parameters. The controller and plant now come from
the digital twin in simulation.twin.
"""

from __future__ import annotations

import argparse
import csv
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from simulation.twin.loop import LoopConfig, run_current_loop  # noqa: E402


def run(duration: float, dt: float, output: Path) -> None:
    result = run_current_loop(LoopConfig(duration_s=duration, dt_s=dt))
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(["time_s", "reference_a", "current_a", "duty"])
        for sample in result.samples:
            writer.writerow([
                f"{sample.time_s:.6f}",
                f"{sample.reference_a:.6f}",
                f"{sample.current_a:.6f}",
                f"{sample.duty:.6f}",
            ])
    final_current = result.samples[-1].current_a
    print(f"wrote {len(result.samples)} samples to {output}")
    print(f"final current: {final_current:.3f} A")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--duration", type=float, default=0.5)
    parser.add_argument("--dt", type=float, default=0.001)
    parser.add_argument("--output", type=Path, default=Path("simulation/output.csv"))
    args = parser.parse_args()
    try:
        run(args.duration, args.dt, args.output)
    except ValueError as exc:
        parser.exit(2, f"error: {exc}\n")


if __name__ == "__main__":
    main()
