#!/usr/bin/env python3
"""Step C: SOGI-PLL lock on U1; PWM released only when locked. No RAMPGEN.

Host model mirrors firmware sogi_pll.c (SOGI + Park q PI). Compares:
  - gated: duty held 0 until lock, then (u_i+U1)/Vbus
  - ungated cheat: applies duty before lock (illegal for product path)
"""

from __future__ import annotations

import argparse
import csv
import math
from dataclasses import dataclass
from pathlib import Path

TWOPI = 2.0 * math.pi


@dataclass
class SogiPll:
    k: float = 1.41421356
    kp: float = 40.0
    ki: float = 800.0
    f0: float = 100.0
    fmin: float = 80.0
    fmax: float = 120.0
    lock_err_max: float = 0.15
    unlock_err_max: float = 0.35
    lock_hold: int = 200
    unlock_hold: int = 100
    umin: float = 1.0
    v_alpha: float = 0.0
    v_beta: float = 0.0
    integ: float = 0.0
    omega: float = TWOPI * 100.0
    theta: float = 0.0
    amp: float = 0.0
    locked: bool = False
    lock_count: int = 0
    unlock_count: int = 0

    def step(self, u1: float, dt: float) -> None:
        d_alpha = self.k * (u1 - self.v_alpha) * self.omega - self.v_beta * self.omega
        d_beta = self.v_alpha * self.omega
        self.v_alpha += d_alpha * dt
        self.v_beta += d_beta * dt
        c, s = math.cos(self.theta), math.sin(self.theta)
        v_q = -s * self.v_alpha + c * self.v_beta
        self.amp = math.hypot(self.v_alpha, self.v_beta)
        self.integ = max(-5000.0, min(5000.0, self.integ + self.ki * v_q * dt))
        omega = TWOPI * self.f0 + self.kp * v_q + self.integ
        self.omega = max(TWOPI * self.fmin, min(TWOPI * self.fmax, omega))
        self.theta = (self.theta + self.omega * dt) % TWOPI
        abs_err_n = (abs(v_q) / self.amp) if self.amp > 1e-3 else 1.0
        candidate = self.amp >= self.umin and abs_err_n <= self.lock_err_max
        bad = self.amp < self.umin or abs_err_n >= self.unlock_err_max
        if self.locked:
            if bad:
                self.unlock_count += 1
                if self.unlock_count >= self.unlock_hold:
                    self.locked = False
                    self.lock_count = 0
                    self.unlock_count = 0
            else:
                self.unlock_count = 0
        else:
            if candidate:
                self.lock_count += 1
                if self.lock_count >= self.lock_hold:
                    self.locked = True
                    self.unlock_count = 0
            else:
                self.lock_count = 0


@dataclass
class PI:
    kp: float = 2.0
    ki: float = 200.0
    vlim: float = 20.0
    ilim: float = 15.0
    integ: float = 0.0

    def step(self, ref: float, meas: float, dt: float) -> float:
        err = ref - meas
        cand = max(-self.ilim, min(self.ilim, self.integ + self.ki * err * dt))
        raw = self.kp * err + cand
        out = max(-self.vlim, min(self.vlim, raw))
        if out == raw or (out > 0 and err < 0) or (out < 0 and err > 0):
            self.integ = cand
        return out


def run(gate: bool, path: Path, duration: float = 0.25, dt: float = 5e-5) -> dict:
    pll = SogiPll()
    pi = PI()
    vbus = 24.0
    iref = 1.0
    i = 0.0
    L, R = 0.002, 0.5
    duty = 0.0
    lock_time = None
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh)
        w.writerow(["time_s", "u1", "theta", "locked", "duty", "current_a", "gate"])
        n = int(duration / dt)
        for k in range(n + 1):
            t = k * dt
            u1 = 8.0 * math.sin(TWOPI * 100.0 * t)
            pll.step(u1, dt)
            if pll.locked and lock_time is None:
                lock_time = t
            u_i = pi.step(iref, i, dt)
            if gate and not pll.locked:
                duty = 0.0
            else:
                duty = max(-0.85, min(0.85, (u_i + u1) / vbus))
            i += (duty * vbus - u1 - R * i) * dt / L
            w.writerow([f"{t:.6f}", f"{u1:.6f}", f"{pll.theta:.6f}", int(pll.locked),
                        f"{duty:.6f}", f"{i:.6f}", int(gate)])
    return {"lock_time_s": lock_time, "final_i": i, "locked": pll.locked, "path": str(path)}


def write_svg(gated: Path, ungated: Path, svg: Path) -> None:
    def load(p):
        with p.open(encoding="utf-8") as fh:
            return list(csv.DictReader(fh))
    g, u = load(gated), load(ungated)
    W, H, L, R, T, B = 900, 420, 60, 20, 30, 40
    tw, th = W - L - R, H - T - B
    tmax = float(g[-1]["time_s"]) or 1.0

    def xy(t, y, ymin=-1.5, ymax=2.5):
        return L + float(t) / tmax * tw, T + (1 - (float(y) - ymin) / (ymax - ymin)) * th

    def poly(rows, key, color, stride=8):
        pts = " ".join(f"{xy(r['time_s'], r[key])[0]:.1f},{xy(r['time_s'], r[key])[1]:.1f}" for r in rows[::stride])
        return f'<polyline fill="none" stroke="{color}" stroke-width="1.4" points="{pts}"/>'

    svg.write_text("\n".join([
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}">',
        '<rect width="100%" height="100%" fill="white"/>',
        f'<text x="{L}" y="18" font-size="14">Step C: PWM gated on SOGI-PLL lock (green) vs ungated cheat (red)</text>',
        poly(g, "current_a", "#1b6"),
        poly(u, "current_a", "#c33"),
        poly(g, "duty", "#15a", 8),
        f'<text x="{W-280}" y="18" font-size="12" fill="#1b6">I gated</text>',
        f'<text x="{W-200}" y="18" font-size="12" fill="#c33">I ungated</text>',
        "</svg>",
    ]), encoding="utf-8")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.parse_args()
    g = run(True, Path("simulation/output_pll_gated.csv"))
    u = run(False, Path("simulation/output_pll_ungated.csv"))
    write_svg(Path("simulation/output_pll_gated.csv"), Path("simulation/output_pll_ungated.csv"),
              Path("simulation/pll_compare.svg"))
    assert g["locked"] and g["lock_time_s"] is not None
    assert g["lock_time_s"] < 0.15
    # Before lock, gated current should stay near 0; ungated moves earlier
    log = Path("simulation/pll_compare_log.txt")
    text = "\n".join([
        "step C SOGI-PLL PWM gate comparison",
        "U1=8 Vpk @ 100 Hz, Vbus=24 V, Iref=1 A, dt=50 us",
        f"gated lock_time_s={g['lock_time_s']:.4f}  final_i={g['final_i']:.4f}",
        f"ungated (cheat) final_i={u['final_i']:.4f}  — illegal product path",
        "sync source: SOGI-PLL on U1 only; no RAMPGEN",
        "PWM duty forced 0 until pll.locked",
        "",
    ])
    log.write_text(text, encoding="utf-8")
    print(text)
    # sample gated CSV: first 500 rows duty must be 0 if not locked
    with Path("simulation/output_pll_gated.csv").open(encoding="utf-8") as fh:
        rows = list(csv.DictReader(fh))
    pre = [r for r in rows if r["locked"] == "0"]
    assert pre and all(abs(float(r["duty"])) < 1e-12 for r in pre)
    print("pre-lock duty all zero: OK")


if __name__ == "__main__":
    main()
