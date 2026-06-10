#!/usr/bin/env python3
"""
CPU Scheduling Simulator — Report Generator
"""

import csv
from typing import Dict, List, Tuple

# Try to import matplotlib; if unavailable, charts are skipped gracefully.
try:
    import matplotlib
    matplotlib.use("Agg")  # non-interactive backend
    import matplotlib.pyplot as plt
    _MATPLOTLIB_AVAILABLE = True
except ImportError:
    _MATPLOTLIB_AVAILABLE = False


def save_csv(results: Dict[str, Dict], filename: str = "report.csv") -> None:
    """Save a summary table to CSV (auto-detects batch / realtime / concurrency)."""
    if not results:
        print("[Report] No results to save.")
        return

    first = next(iter(results.values()))
    if "deadline_miss_rate" in first:
        fieldnames = ["Algorithm", "Total_Jobs", "Missed_Jobs", "Miss_Rate",
                      "Max_Response_Time", "Avg_Response_Time", "Jitter"]
        with open(filename, "w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            for algo_name, m in results.items():
                writer.writerow({
                    "Algorithm": algo_name,
                    "Total_Jobs": m.get("total_jobs", ""),
                    "Missed_Jobs": m.get("missed_jobs", ""),
                    "Miss_Rate": m.get("deadline_miss_rate", ""),
                    "Max_Response_Time": m.get("max_response_time", ""),
                    "Avg_Response_Time": m.get("avg_response_time", ""),
                    "Jitter": m.get("jitter", ""),
                })
    elif "speedup" in first:
        fieldnames = ["Algorithm", "Speedup", "Migration_Count", "Avg_Migrations",
                      "Total_Burst", "Total_Time", "Num_CPUs"]
        with open(filename, "w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            for algo_name, m in results.items():
                writer.writerow({
                    "Algorithm": algo_name,
                    "Speedup": m.get("speedup", ""),
                    "Migration_Count": m.get("migration_count", ""),
                    "Avg_Migrations": m.get("avg_migrations", ""),
                    "Total_Burst": m.get("total_burst", ""),
                    "Total_Time": m.get("total_time", ""),
                    "Num_CPUs": m.get("num_cpus", ""),
                })
    else:
        fieldnames = ["Algorithm", "Avg_Turnaround", "Avg_Waiting",
                      "Avg_Weighted_Turnaround", "Fairness_StdDev",
                      "Fairness_Jain", "Throughput"]
        with open(filename, "w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            for algo_name, m in results.items():
                writer.writerow({
                    "Algorithm": algo_name,
                    "Avg_Turnaround": m.get("avg_turnaround_time", ""),
                    "Avg_Waiting": m.get("avg_waiting_time", ""),
                    "Avg_Weighted_Turnaround": m.get("avg_weighted_turnaround", ""),
                    "Fairness_StdDev": m.get("fairness_stddev", ""),
                    "Fairness_Jain": m.get("fairness_jain", ""),
                    "Throughput": m.get("throughput", ""),
                })
    print(f"[Report] CSV saved to {filename}")


def _detect_gantt_format(gantt: List[Tuple]) -> Tuple[int, bool]:
    """
    Scan all gantt entries and return (num_cpus, has_missed_flag).
    """
    max_cpu = 0
    has_missed = False
    for entry in gantt:
        # entry formats:
        #   (pid, start, end)                  -> single-core batch
        #   (pid, start, end, missed)          -> single-core realtime
        #   (pid, start, end, cpu_id)          -> multi-core batch
        #   (pid, start, end, missed, cpu_id)  -> multi-core realtime
        if len(entry) == 5:
            max_cpu = max(max_cpu, entry[4])
            has_missed = has_missed or entry[3]
        elif len(entry) == 4:
            last = entry[3]
            if isinstance(last, int):
                max_cpu = max(max_cpu, last)
            elif isinstance(last, bool):
                has_missed = has_missed or last
    return max_cpu + 1, has_missed


def plot_gantt(
    gantt_data: Dict[str, List[Tuple]],
    filename: str = "gantt_chart.png",
    title: str = "Scheduling Comparison",
) -> None:
    """
    Draw a Gantt-style chart.

    Auto-detects single-core vs multi-core and batch vs realtime formats.
    Multi-core: each CPU core gets its own row within the algorithm's panel.
    Missed deadlines are drawn in RED.
    """
    if not _MATPLOTLIB_AVAILABLE:
        print("[Report] Matplotlib not installed; skipping chart generation.")
        return

    algo_names = list(gantt_data.keys())
    n_algos = len(algo_names)

    # Detect max CPUs across all algorithms
    max_cpus = 1
    for gantt in gantt_data.values():
        n, _ = _detect_gantt_format(gantt)
        max_cpus = max(max_cpus, n)

    # Build colour map for PIDs
    all_pids = set()
    for gantt in gantt_data.values():
        for entry in gantt:
            pid = entry[0]
            if pid is not None:
                all_pids.add(pid)
    cmap = plt.cm.get_cmap("tab20", max(len(all_pids), 1))
    pid_to_color = {pid: cmap(i) for i, pid in enumerate(sorted(all_pids))}

    fig, axes = plt.subplots(
        nrows=n_algos,
        ncols=1,
        figsize=(12, 1.2 * max_cpus * n_algos),
        sharex=True,
    )
    if n_algos == 1:
        axes = [axes]

    for ax, algo_name in zip(axes, algo_names):
        gantt = gantt_data[algo_name]
        n_cpus, has_missed = _detect_gantt_format(gantt)

        # Group gantt entries by CPU
        cpu_entries: Dict[int, List[Tuple]] = {i: [] for i in range(n_cpus)}
        for entry in gantt:
            if len(entry) == 5:
                pid, start, end, missed, cpu_id = entry
                cpu_entries[cpu_id].append((pid, start, end, missed))
            elif len(entry) == 4 and isinstance(entry[3], int):
                pid, start, end, cpu_id = entry
                cpu_entries[cpu_id].append((pid, start, end, False))
            elif len(entry) == 4 and isinstance(entry[3], bool):
                pid, start, end, missed = entry
                cpu_entries[0].append((pid, start, end, missed))
            else:
                pid, start, end = entry
                cpu_entries[0].append((pid, start, end, False))

        y_positions = list(range(n_cpus))
        y_labels = [f"CPU {i}" for i in range(n_cpus)]

        for cpu_id in range(n_cpus):
            y = y_positions[cpu_id]
            for seg in cpu_entries[cpu_id]:
                pid, start, end, missed = seg
                if missed:
                    color = "red"
                    edgecolor = "darkred"
                    linewidth = 2.0
                else:
                    color = pid_to_color.get(pid, "lightgray") if pid is not None else "white"
                    edgecolor = "black"
                    linewidth = 0.5

                ax.barh(
                    y=y,
                    width=end - start,
                    left=start,
                    height=0.6,
                    color=color,
                    edgecolor=edgecolor,
                    linewidth=linewidth,
                )
                if (end - start) >= 1.5 and pid is not None:
                    ax.text(
                        start + (end - start) / 2,
                        y,
                        f"P{pid}",
                        ha="center",
                        va="center",
                        fontsize=6,
                        color="white" if not missed else "yellow",
                    )

        ax.set_yticks(y_positions)
        ax.set_yticklabels(y_labels)
        ax.set_ylim(-0.6, n_cpus - 0.4)
        ax.set_xlim(left=0)
        ax.grid(axis="x", linestyle="--", alpha=0.4)
        ax.set_ylabel(algo_name, rotation=0, ha="right", va="center", fontsize=10, fontweight="bold")

    axes[-1].set_xlabel("Time")
    fig.suptitle(title, fontsize=14, fontweight="bold")
    plt.tight_layout(rect=[0, 0.03, 1, 0.95])
    plt.savefig(filename, dpi=150)
    plt.close()
    print(f"[Report] Gantt chart saved to {filename}")
