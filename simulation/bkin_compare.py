#!/usr/bin/env python3
"""Step F: BKIN trip clears MOE before software; AOE=0; PB12 is not BKIN."""

from __future__ import annotations

import csv
from pathlib import Path


def run():
    # Discrete event log: hardware break at t=0.05, software tries clear too early, then after lock
    rows = []
    moe = 1
    trip = 0
    pll = 0
    pb12 = 1  # GPIO high = ok; not wired as BKIN
    duty = 0.4
    for i in range(0, 201):
        t = i * 0.001
        if t >= 0.02:
            pll = 1
        if abs(t - 0.05) < 1e-12:
            # hardware BKIN: MOE cleared first
            moe = 0
            trip = 1
            duty = 0.0
        # illegal early software clear at 0.06 while pretending PB12 poll is enough
        if abs(t - 0.06) < 1e-12 and trip:
            # PB12-only clear rejected: need pll+safe AND explicit MOE rearm; still tripped
            pass
        if abs(t - 0.12) < 1e-12 and trip and pll:
            trip = 0
            # AOE=0 => MOE still 0 until explicit rearm
            moe = 0
        if abs(t - 0.13) < 1e-12 and not trip:
            moe = 1
            duty = 0.3
        out = duty if moe and not trip else 0.0
        rows.append((t, moe, trip, pll, pb12, out))

    path = Path("simulation/output_bkin.csv")
    with path.open("w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh)
        w.writerow(["time_s", "moe", "trip", "pll", "pb12", "pwm_out"])
        for r in rows:
            w.writerow([f"{r[0]:.3f}", *r[1:]])

    # Assertions for recreate
    at = {f"{r[0]:.3f}": r for r in rows}
    assert at["0.049"][1] == 1  # moe before
    assert at["0.050"][1] == 0 and at["0.050"][2] == 1  # moe clear on break
    assert at["0.050"][5] == 0.0
    assert at["0.120"][2] == 0 and at["0.120"][1] == 0  # cleared trip, MOE still 0 (AOE=0)
    assert at["0.130"][1] == 1 and at["0.130"][5] > 0

    # SVG
    W, H, L, R, T, B = 900, 300, 50, 20, 30, 40
    tw, th = W - L - R, H - T - B
    tmax = rows[-1][0] or 1.0

    def xy(t, y):
        return L + t / tmax * tw, T + (1 - y) * th

    def poly(idx, color):
        pts = " ".join(f"{xy(r[0], r[idx])[0]:.1f},{xy(r[0], r[idx])[1]:.1f}" for r in rows)
        return f'<polyline fill="none" stroke="{color}" stroke-width="1.6" points="{pts}"/>'

    Path("simulation/bkin_compare.svg").write_text("\n".join([
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}">',
        '<rect width="100%" height="100%" fill="white"/>',
        f'<text x="{L}" y="18" font-size="14">Step F: BKIN clears MOE (blue) before soft clear; AOE=0</text>',
        poly(1, "#15a"),  # moe
        poly(2, "#c33"),  # trip
        poly(5, "#1b6"),  # pwm
        f'<text x="{W-260}" y="18" font-size="12" fill="#15a">MOE</text>',
        f'<text x="{W-200}" y="18" font-size="12" fill="#c33">trip</text>',
        f'<text x="{W-140}" y="18" font-size="12" fill="#1b6">pwm</text>',
        "</svg>",
    ]), encoding="utf-8")

    log = Path("simulation/bkin_compare_log.txt")
    text = "\n".join([
        "step F TIM1 BKIN / MOE",
        "PA6 TIM1_BKIN active-low (BKP=0), AOE=0, BKE=1",
        "at t=0.050 hardware break: MOE→0, trip latched, pwm→0",
        "at t=0.120 soft clear after PLL: trip cleared, MOE still 0 (AOE=0)",
        "at t=0.130 explicit MOE re-arm",
        "PB12 GPIO is not BKIN and cannot substitute",
        "",
    ])
    log.write_text(text, encoding="utf-8")
    print(text)


if __name__ == "__main__":
    run()
