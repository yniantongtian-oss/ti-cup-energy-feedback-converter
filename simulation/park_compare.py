#!/usr/bin/env python3
"""Step D: Park + Iq=0 (dual PI) vs stationary PI. No harmonic PR.

Plant (averaged): L di_alpha/dt = duty*Vbus - U1 - R*i_alpha
i_beta from SOGI on i_alpha. Theta from ideal angle (=U1 phase) for host clarity
(firmware uses SOGI-PLL theta; here isolate Park/Iq=0 effect).
"""

from __future__ import annotations

import argparse
import csv
import math
from pathlib import Path

TWOPI = 2.0 * math.pi


def sogi_step(alpha, beta, u, w, k, dt):
    d_a = k * (u - alpha) * w - beta * w
    d_b = alpha * w
    return alpha + d_a * dt, beta + d_b * dt


def pi(integ, kp, ki, err, dt, lim=20.0, ilim=15.0):
    cand = max(-ilim, min(ilim, integ + ki * err * dt))
    raw = kp * err + cand
    out = max(-lim, min(lim, raw))
    if out == raw or (out > 0 and err < 0) or (out < 0 and err > 0):
        integ = cand
    return out, integ


def run(use_park: bool, path: Path, duration=0.3, dt=5e-5) -> dict:
    vbus, L, R, k = 24.0, 0.002, 0.5, 1.41421356
    f, amp_u1 = 100.0, 8.0
    id_ref = 1.0
    i_a = 0.0
    i_sa = i_sb = 0.0
    integ_d = integ_q = integ_s = 0.0
    w = TWOPI * f
    iq_sq = 0.0
    n = 0
    settle = 0.15
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as fh:
        wri = csv.writer(fh)
        wri.writerow(["time_s", "i_alpha", "id", "iq", "duty", "use_park"])
        steps = int(duration / dt)
        for kstep in range(steps + 1):
            t = kstep * dt
            theta = (w * t) % TWOPI
            u1 = amp_u1 * math.sin(theta)
            i_sa, i_sb = sogi_step(i_sa, i_sb, i_a, w, k, dt)
            c, s = math.cos(theta), math.sin(theta)
            id_m = c * i_a + s * i_sb
            iq_m = -s * i_a + c * i_sb
            if use_park:
                ud, integ_d = pi(integ_d, 8.0, 400.0, id_ref - id_m, dt)
                uq, integ_q = pi(integ_q, 8.0, 400.0, 0.0 - iq_m, dt)
                u_alpha = c * ud - s * uq
            else:
                # stationary PI on i_alpha toward id_ref*cos(theta) would be AC tracking —
                # fair compare: regulate i_alpha to id_ref (DC) while U1 is AC disturbance
                u_alpha, integ_s = pi(integ_s, 8.0, 400.0, id_ref - i_a, dt)
                integ_q = 0.0
            duty = max(-0.85, min(0.85, (u_alpha + u1) / vbus))
            i_a += (duty * vbus - u1 - R * i_a) * dt / L
            wri.writerow([f"{t:.6f}", f"{i_a:.6f}", f"{id_m:.6f}", f"{iq_m:.6f}",
                          f"{duty:.6f}", int(use_park)])
            if t >= settle:
                iq_sq += iq_m * iq_m
                n += 1
    iq_rms = math.sqrt(iq_sq / n) if n else float("nan")
    print(f"wrote {path}  park={use_park}  Iq_rms={iq_rms:.4f} A  final_i_alpha={i_a:.3f}")
    return {"iq_rms": iq_rms, "final_i": i_a}


def write_svg(park: Path, stat: Path, svg: Path) -> None:
    def load(p):
        with p.open(encoding="utf-8") as fh:
            return list(csv.DictReader(fh))
    a, b = load(park), load(stat)
    W, H, L, R, T, B = 900, 420, 60, 20, 30, 40
    tw, th = W - L - R, H - T - B
    tmax = float(a[-1]["time_s"]) or 1.0

    def xy(t, y, ymin=-1.5, ymax=2.0):
        return L + float(t) / tmax * tw, T + (1 - (float(y) - ymin) / (ymax - ymin)) * th

    def poly(rows, key, color, stride=10):
        pts = " ".join(f"{xy(r['time_s'], r[key])[0]:.1f},{xy(r['time_s'], r[key])[1]:.1f}" for r in rows[::stride])
        return f'<polyline fill="none" stroke="{color}" stroke-width="1.4" points="{pts}"/>'

    svg.write_text("\n".join([
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}">',
        '<rect width="100%" height="100%" fill="white"/>',
        f'<text x="{L}" y="18" font-size="14">Step D: Iq (Park+Iq=0 green) vs stationary PI (red)</text>',
        poly(a, "iq", "#1b6"),
        poly(b, "iq", "#c33"),
        f'<text x="{W-260}" y="18" font-size="12" fill="#1b6">Iq Park</text>',
        f'<text x="{W-160}" y="18" font-size="12" fill="#c33">Iq stationary</text>',
        "</svg>",
    ]), encoding="utf-8")


def main():
    argparse.ArgumentParser(description=__doc__).parse_args()
    p = run(True, Path("simulation/output_park_on.csv"))
    s = run(False, Path("simulation/output_park_off.csv"))
    write_svg(Path("simulation/output_park_on.csv"), Path("simulation/output_park_off.csv"),
              Path("simulation/park_compare.svg"))
    ratio = s["iq_rms"] / p["iq_rms"] if p["iq_rms"] > 1e-6 else float("inf")
    text = "\n".join([
        "step D Park+Iq=0 comparison",
        "U1=8 Vpk @ 100 Hz, Vbus=24, Id_ref=1 A, dual PI, no PR",
        f"Park ON  Iq_rms={p['iq_rms']:.6f} A",
        f"Park OFF Iq_rms={s['iq_rms']:.6f} A",
        f"OFF/ON Iq_rms ratio={ratio:.2f} ( >1 means Park+Iq=0 better )",
        "",
    ])
    Path("simulation/park_compare_log.txt").write_text(text, encoding="utf-8")
    print(text)
    if not (s["iq_rms"] > p["iq_rms"] * 1.5):
        raise SystemExit("expected Park+Iq=0 to reduce Iq RMS vs stationary PI")


if __name__ == "__main__":
    main()
