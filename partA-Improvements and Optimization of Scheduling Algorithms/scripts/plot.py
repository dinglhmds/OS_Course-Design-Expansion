#!/usr/bin/env python3
import csv
import sys
from pathlib import Path

COLORS = {
    "fcfs": "#4C78A8",
    "sjf": "#F58518",
    "srtf": "#54A24B",
    "rr": "#E45756",
    "priority": "#72B7B2",
    "mlfq": "#B279A2",
    "aamlfq": "#FF9DA6",
    "qlearn": "#9D755D",
}

METRICS = [
    ("avg_turnaround", "Average Turnaround Time", "lower is better"),
    ("avg_response", "Average Response Time", "lower is better"),
    ("deadline_misses", "Deadline Misses", "lower is better"),
    ("fairness", "Fairness Index", "higher is better"),
]


def esc(text):
    return str(text).replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace('"', "&quot;")


def draw_metric(rows, metric, title, subtitle, out_path):
    workloads = sorted({row["workload"] for row in rows})
    algorithms = ["fcfs", "sjf", "srtf", "rr", "priority", "mlfq", "aamlfq", "qlearn"]
    lookup = {(row["workload"], row["algorithm"]): row for row in rows}

    width = 1220
    height = 720
    margin_left = 86
    margin_right = 36
    margin_top = 96
    margin_bottom = 126
    plot_w = width - margin_left - margin_right
    plot_h = height - margin_top - margin_bottom

    vals = [float(row[metric]) for row in rows]
    max_v = max(vals) if vals else 1.0
    if max_v <= 0:
        max_v = 1.0
    y_max = max_v * 1.15
    if metric == "fairness":
        y_max = 1.0

    group_gap = 28
    group_w = (plot_w - group_gap * (len(workloads) - 1)) / len(workloads)
    bar_gap = 4
    bar_w = (group_w - bar_gap * (len(algorithms) - 1)) / len(algorithms)

    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="#fbfbf8"/>',
        f'<text x="{margin_left}" y="42" font-family="Arial, sans-serif" font-size="26" font-weight="700" fill="#242424">{esc(title)}</text>',
        f'<text x="{margin_left}" y="70" font-family="Arial, sans-serif" font-size="14" fill="#666">{esc(subtitle)}</text>',
    ]

    for i in range(6):
        tick = y_max * i / 5
        y = margin_top + plot_h - (tick / y_max) * plot_h
        parts.append(f'<line x1="{margin_left}" y1="{y:.1f}" x2="{width - margin_right}" y2="{y:.1f}" stroke="#e1e1dc" stroke-width="1"/>')
        parts.append(f'<text x="{margin_left - 10}" y="{y + 4:.1f}" text-anchor="end" font-family="Arial, sans-serif" font-size="12" fill="#666">{tick:.2f}</text>')

    parts.append(f'<line x1="{margin_left}" y1="{margin_top}" x2="{margin_left}" y2="{margin_top + plot_h}" stroke="#333" stroke-width="1.2"/>')
    parts.append(f'<line x1="{margin_left}" y1="{margin_top + plot_h}" x2="{width - margin_right}" y2="{margin_top + plot_h}" stroke="#333" stroke-width="1.2"/>')

    for wi, workload in enumerate(workloads):
        x0 = margin_left + wi * (group_w + group_gap)
        label_x = x0 + group_w / 2
        parts.append(f'<text x="{label_x:.1f}" y="{margin_top + plot_h + 32}" text-anchor="middle" font-family="Arial, sans-serif" font-size="13" fill="#333">{esc(workload)}</text>')
        for ai, algo in enumerate(algorithms):
            row = lookup.get((workload, algo))
            if not row:
                continue
            v = float(row[metric])
            bar_h = (v / y_max) * plot_h
            x = x0 + ai * (bar_w + bar_gap)
            y = margin_top + plot_h - bar_h
            color = COLORS.get(algo, "#999")
            parts.append(f'<rect x="{x:.1f}" y="{y:.1f}" width="{bar_w:.1f}" height="{bar_h:.1f}" rx="3" fill="{color}">')
            parts.append(f'<title>{esc(workload)} {esc(algo)}: {v:.3f}</title>')
            parts.append('</rect>')

    legend_y = height - 54
    legend_x = margin_left
    for algo in algorithms:
        color = COLORS.get(algo, "#999")
        parts.append(f'<rect x="{legend_x}" y="{legend_y - 12}" width="14" height="14" rx="2" fill="{color}"/>')
        parts.append(f'<text x="{legend_x + 20}" y="{legend_y}" font-family="Arial, sans-serif" font-size="13" fill="#333">{esc(algo)}</text>')
        legend_x += 94

    parts.append('</svg>')
    out_path.write_text('\n'.join(parts), encoding="utf-8")


def write_index(metric_files, out_path):
    lines = ["# Benchmark Charts", ""]
    for title, path in metric_files:
        rel = Path("..") / "figures" / path.name
        lines.append(f"## {title}")
        lines.append("")
        lines.append(f"![{title}]({rel.as_posix()})")
        lines.append("")
    out_path.write_text('\n'.join(lines), encoding="utf-8")


def main():
    if len(sys.argv) not in (2, 3):
        raise SystemExit("usage: plot.py results/raw.csv [figures_dir]")
    raw = Path(sys.argv[1])
    figures = Path(sys.argv[2]) if len(sys.argv) == 3 else raw.parents[1] / "figures"
    figures.mkdir(parents=True, exist_ok=True)
    rows = list(csv.DictReader(raw.open(encoding="utf-8")))
    if not rows:
        raise SystemExit("empty raw csv")

    metric_files = []
    for metric, title, note in METRICS:
        out = figures / f"{metric}.svg"
        draw_metric(rows, metric, title, note, out)
        metric_files.append((title, out))
        print(f"wrote {out}")

    write_index(metric_files, raw.parent / "charts.md")
    print(f"wrote {raw.parent / 'charts.md'}")


if __name__ == "__main__":
    main()
