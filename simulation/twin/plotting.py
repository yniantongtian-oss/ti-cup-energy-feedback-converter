"""Small dependency-free SVG plots for twin waveforms."""

from __future__ import annotations

import math
from pathlib import Path


COLORS = ("#0f6f8a", "#c4491d", "#2f6b3a", "#6b4c9a", "#8a6a12")


def _escape(text: str) -> str:
    return (
        text.replace("&", "&amp;")
        .replace("<", "&lt;")
        .replace(">", "&gt;")
        .replace('"', "&quot;")
    )


def _span(values: list[float]) -> tuple[float, float]:
    finite = [value for value in values if math.isfinite(value)]
    if not finite:
        return 0.0, 1.0
    low = min(finite)
    high = max(finite)
    if high - low < 1e-9:
        pad = 1.0 if abs(low) < 1e-9 else abs(low) * 0.1
        return low - pad, high + pad
    pad = 0.05 * (high - low)
    return low - pad, high + pad


def write_series_svg(
    path: Path,
    x_values: list[float],
    series: dict[str, list[float]],
    *,
    title: str,
    x_label: str = "time (s)",
    y_label: str = "value",
) -> None:
    if not series:
        raise ValueError("plot requires at least one series")
    if not x_values:
        raise ValueError("plot requires samples")
    width = len(x_values)
    for name, values in series.items():
        if len(values) != width:
            raise ValueError(f"series {name} length does not match the time base")

    left, top, right, bottom = 70, 48, 760, 390
    plot_width = right - left
    plot_height = bottom - top
    x_low, x_high = _span(x_values)
    parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<svg xmlns="http://www.w3.org/2000/svg" width="800" height="460" viewBox="0 0 800 460">',
        "<title>" + _escape(title) + "</title>",
        '<rect width="800" height="460" fill="#f7f4ee"/>',
        f'<text x="70" y="30" font-family="sans-serif" font-size="16" fill="#1c1a17">{_escape(title)}</text>',
        f'<rect x="{left}" y="{top}" width="{plot_width}" height="{plot_height}" fill="#fffdf8" stroke="#d9d2c5"/>',
    ]
    for step in range(5):
        y = top + plot_height * step / 4
        x = left + plot_width * step / 4
        parts.append(f'<line x1="{left}" y1="{y:.1f}" x2="{right}" y2="{y:.1f}" stroke="#eee6d8"/>')
        parts.append(f'<line x1="{x:.1f}" y1="{top}" x2="{x:.1f}" y2="{bottom}" stroke="#eee6d8"/>')

    legend_y = 418
    for index, (name, values) in enumerate(series.items()):
        low, high = _span(values)
        color = COLORS[index % len(COLORS)]
        coords = []
        for x_value, y_value in zip(x_values, values):
            if not math.isfinite(x_value) or not math.isfinite(y_value):
                continue
            px = left + (x_value - x_low) / (x_high - x_low) * plot_width
            py = bottom - (y_value - low) / (high - low) * plot_height
            coords.append(f"{px:.2f},{py:.2f}")
        if len(coords) >= 2:
            parts.append(
                f'<polyline fill="none" stroke="{color}" stroke-width="1.8" points="{" ".join(coords)}"/>'
            )
        legend_x = 70 + (index % 3) * 230
        row = legend_y + (index // 3) * 18
        parts.append(f'<line x1="{legend_x}" y1="{row}" x2="{legend_x + 18}" y2="{row}" stroke="{color}" stroke-width="2"/>')
        parts.append(
            f'<text x="{legend_x + 24}" y="{row + 4}" font-family="sans-serif" font-size="12" fill="#1c1a17">'
            f'{_escape(name)} [{low:.3g}, {high:.3g}]</text>'
        )

    parts.append(
        f'<text x="400" y="448" text-anchor="middle" font-family="sans-serif" font-size="12" fill="#5c564c">{_escape(x_label)}</text>'
    )
    parts.append(
        f'<text x="18" y="220" transform="rotate(-90 18 220)" font-family="sans-serif" font-size="12" fill="#5c564c">{_escape(y_label)}</text>'
    )
    parts.append("</svg>")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(parts) + "\n", encoding="utf-8")
