#!/usr/bin/env python3
import csv
import sys
from collections import defaultdict
from pathlib import Path


def fmt(value):
    return f"{float(value):.3f}"


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: summarize.py results/raw.csv")
    path = Path(sys.argv[1])
    rows = list(csv.DictReader(path.open()))
    if not rows:
        raise SystemExit("empty results")

    by_workload = defaultdict(list)
    for row in rows:
        by_workload[row["workload"]].append(row)

    print("# Scheduling Benchmark Summary")
    print()
    print("Lower turnaround, waiting, response, slowdown, deadline misses, and context switches are better.")
    print("Higher throughput, CPU utilization, and fairness are better.")
    print()

    for workload in sorted(by_workload):
        group = by_workload[workload]
        print(f"## {workload}")
        print()
        print("| algorithm | avg turnaround | avg waiting | avg response | slowdown | misses | ctx switches | fairness |")
        print("|---|---:|---:|---:|---:|---:|---:|---:|")
        for row in sorted(group, key=lambda r: float(r["avg_turnaround"])):
            print(
                "| {algorithm} | {tat} | {wait} | {resp} | {slow} | {miss} | {ctx} | {fair} |".format(
                    algorithm=row["algorithm"],
                    tat=fmt(row["avg_turnaround"]),
                    wait=fmt(row["avg_waiting"]),
                    resp=fmt(row["avg_response"]),
                    slow=fmt(row["avg_slowdown"]),
                    miss=row["deadline_misses"],
                    ctx=row["context_switches"],
                    fair=fmt(row["fairness"]),
                )
            )
        best_response = min(group, key=lambda r: float(r["avg_response"]))
        best_turnaround = min(group, key=lambda r: float(r["avg_turnaround"]))
        fewest_misses = min(group, key=lambda r: (float(r["deadline_misses"]), float(r["avg_turnaround"])))
        print()
        print(
            f"Best response: {best_response['algorithm']}; "
            f"best turnaround: {best_turnaround['algorithm']}; "
            f"fewest deadline misses: {fewest_misses['algorithm']}."
        )
        print()


if __name__ == "__main__":
    main()
