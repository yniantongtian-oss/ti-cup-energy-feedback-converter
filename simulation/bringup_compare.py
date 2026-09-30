#!/usr/bin/env python3
"""Step E: bring-up sequence + bus slow ramp (host model)."""

from __future__ import annotations

import csv
import math
from pathlib import Path

TWOPI = 2.0 * math.pi

# Mirror bringup.c defaults (shortened dwells for sim)
OPEN_MS, DC_MS = 50, 80
BUS_START, BUS_TARGET, BUS_RAMP = 12.0, 24.0, 8.0  # V, V/s


def main():
    dt = 0.001
    duration = 4.0
    mode = "IDLE"
    elapsed = 0.0
    bus_cmd = BUS_START
    id_ref = 0.0
    outer_i = 0.0
    v_out = 0.0
    locked = False
    path = Path("simulation/output_bringup.csv")
    modes_seen = []
    with path.open("w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh)
        w.writerow(["time_s", "mode", "bus_cmd_v", "id_ref_a", "v_out", "locked"])
        t = 0.0
        while t <= duration + 1e-12:
            # PLL locks after 40 ms of "U1 present"
            locked = t >= 0.04
            # fake outer voltage rises when current stage on
            if mode in ("CURRENT_DC", "OUTER_V"):
                v_out += (8.0 - v_out) * 0.02
            else:
                v_out *= 0.99

            if mode == "IDLE":
                mode = "OPEN_LOOP"
                elapsed = 0.0
            elif mode == "OPEN_LOOP":
                id_ref = 0.0
                if locked and elapsed >= OPEN_MS / 1000.0:
                    mode = "CURRENT_DC"
                    elapsed = 0.0
            elif mode == "CURRENT_DC":
                id_ref = 0.5
                if locked and elapsed >= DC_MS / 1000.0:
                    mode = "OUTER_V"
                    elapsed = 0.0
            elif mode == "OUTER_V":
                err = 8.0 - v_out
                outer_i = max(-2.0, min(2.0, outer_i + 0.5 * err * dt))
                id_ref = max(-2.0, min(2.0, 0.05 * err + outer_i))

            # bus ramp always while running
            if mode != "IDLE":
                max_d = BUS_RAMP * dt
                errb = BUS_TARGET - bus_cmd
                if abs(errb) <= max_d:
                    bus_cmd = BUS_TARGET
                else:
                    bus_cmd += math.copysign(max_d, errb)

            if not modes_seen or modes_seen[-1] != mode:
                modes_seen.append(mode)
            w.writerow([f"{t:.3f}", mode, f"{bus_cmd:.4f}", f"{id_ref:.4f}", f"{v_out:.4f}", int(locked)])
            elapsed += dt
            t += dt

    # SVG mode timeline
    svg = Path("simulation/bringup_compare.svg")
    rows = list(csv.DictReader(path.open(encoding="utf-8")))
    W, H, L, R, T, B = 900, 320, 60, 20, 40, 40
    tw, th = W - L - R, H - T - B
    tmax = float(rows[-1]["time_s"]) or 1.0
    mode_y = {"IDLE": 0.2, "OPEN_LOOP": 0.4, "CURRENT_DC": 0.6, "OUTER_V": 0.8, "FAULT": 0.0}

    def xy(t, y):
        return L + float(t) / tmax * tw, T + (1 - y) * th

    def poly(key, color, ymin, ymax, stride=5):
        pts = []
        for r in rows[::stride]:
            v = float(r[key])
            yn = (v - ymin) / (ymax - ymin)
            pts.append(f"{xy(r['time_s'], yn)[0]:.1f},{xy(r['time_s'], yn)[1]:.1f}")
        return f'<polyline fill="none" stroke="{color}" stroke-width="1.5" points="{" ".join(pts)}"/>'

    mode_pts = []
    for r in rows[::5]:
        mode_pts.append(f"{xy(r['time_s'], mode_y.get(r['mode'], 0))[0]:.1f},{xy(r['time_s'], mode_y.get(r['mode'], 0))[1]:.1f}")
    svg.write_text("\n".join([
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}">',
        '<rect width="100%" height="100%" fill="white"/>',
        f'<text x="{L}" y="22" font-size="14">Step E: bring-up modes + bus_cmd ramp</text>',
        f'<polyline fill="none" stroke="#333" stroke-width="2" points="{" ".join(mode_pts)}"/>',
        poly("bus_cmd_v", "#15a", 10, 26, 5),
        poly("id_ref_a", "#1b6", -0.5, 2.5, 5),
        f'<text x="{W-280}" y="22" font-size="12" fill="#333">mode</text>',
        f'<text x="{W-220}" y="22" font-size="12" fill="#15a">bus_cmd</text>',
        f'<text x="{W-140}" y="22" font-size="12" fill="#1b6">id_ref</text>',
        "</svg>",
    ]), encoding="utf-8")

    assert modes_seen[:4] == ["OPEN_LOOP", "CURRENT_DC", "OUTER_V"] or modes_seen == ["OPEN_LOOP", "CURRENT_DC", "OUTER_V"]
    # bus reaches target
    assert abs(float(rows[-1]["bus_cmd_v"]) - BUS_TARGET) < 0.05
    # early bus near start
    early = next(r for r in rows if float(r["time_s"]) >= 0.05)
    assert float(early["bus_cmd_v"]) < BUS_START + 1.0
    log = Path("simulation/bringup_compare_log.txt")
    text = "\n".join([
        "step E bring-up sequence",
        f"modes_seen={modes_seen}",
        f"bus_start={BUS_START} bus_target={BUS_TARGET} ramp={BUS_RAMP} V/s",
        f"final bus_cmd={rows[-1]['bus_cmd_v']} final mode={rows[-1]['mode']}",
        "OPEN_LOOP → CURRENT_DC (DC Id) → OUTER_V; bus slow ramp",
        "F (BKIN) not included",
        "",
    ])
    log.write_text(text, encoding="utf-8")
    print(text)


if __name__ == "__main__":
    main()
