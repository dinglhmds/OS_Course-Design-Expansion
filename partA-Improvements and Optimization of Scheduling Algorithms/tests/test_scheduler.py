#!/usr/bin/env python3
import csv
import math
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BIN = ROOT / "scheduler"


HEADER = [
    "algorithm",
    "workload",
    "processes",
    "makespan",
    "avg_turnaround",
    "avg_waiting",
    "avg_response",
    "avg_slowdown",
    "throughput",
    "context_switches",
    "deadline_misses",
    "cpu_utilization",
    "fairness",
]


def run_scheduler(algo, workload, *extra):
    cmd = [
        str(BIN),
        "--algo",
        algo,
        "--input",
        str(workload),
        "--workload",
        workload.stem,
        "--csv",
        *extra,
    ]
    out = subprocess.check_output(cmd, text=True, cwd=ROOT).strip()
    row = next(csv.DictReader([",".join(HEADER), out]))
    for key in HEADER[2:]:
        row[key] = float(row[key])
    return row


def assert_close(actual, expected, eps=1e-6):
    if not math.isclose(actual, expected, rel_tol=eps, abs_tol=eps):
        raise AssertionError(f"expected {expected}, got {actual}")


def test_fcfs_known_metrics():
    row = run_scheduler("fcfs", ROOT / "tests" / "basic.csv")
    assert row["processes"] == 3
    assert row["makespan"] == 6
    assert_close(row["avg_turnaround"], 8.0 / 3.0)
    assert_close(row["avg_waiting"], 2.0 / 3.0)
    assert_close(row["avg_response"], 2.0 / 3.0)
    assert row["deadline_misses"] == 0


def test_all_algorithms_keep_core_invariants():
    workload = ROOT / "workloads" / "cpu_mixed.csv"
    algos = ["fcfs", "sjf", "srtf", "rr", "priority", "mlfq", "aamlfq", "qlearn"]
    for algo in algos:
        row = run_scheduler(algo, workload)
        assert row["processes"] == 6
        assert row["makespan"] >= 58
        assert 0.0 < row["throughput"] <= 1.0
        assert 0.0 < row["cpu_utilization"] <= 1.0
        assert 0.0 < row["fairness"] <= 1.0
        assert row["avg_turnaround"] >= row["avg_response"]
        assert row["context_switches"] >= 0


def test_mlfq_improves_interactive_response_over_fcfs():
    workload = ROOT / "workloads" / "interactive_io.csv"
    fcfs = run_scheduler("fcfs", workload)
    mlfq = run_scheduler("mlfq", workload, "--quantum", "3", "--aging", "12", "--boost", "48")
    assert mlfq["avg_response"] < fcfs["avg_response"]
    assert mlfq["fairness"] >= 0.65


def test_qlearn_is_adaptive_on_interactive_workload():
    workload = ROOT / "workloads" / "interactive_io.csv"
    fcfs = run_scheduler("fcfs", workload)
    qlearn = run_scheduler(
        "qlearn",
        workload,
        "--quantum",
        "3",
        "--aging",
        "12",
        "--boost",
        "48",
        "--rl-alpha",
        "0.30",
        "--rl-gamma",
        "0.85",
        "--rl-epsilon",
        "0.05",
    )
    assert qlearn["avg_response"] < fcfs["avg_response"]
    assert qlearn["fairness"] >= 0.65


def test_trace_and_process_exports():
    with tempfile.TemporaryDirectory() as tmp:
        trace = Path(tmp) / "trace.csv"
        procs = Path(tmp) / "processes.csv"
        workload = ROOT / "tests" / "basic.csv"
        row = run_scheduler("rr", workload, "--trace", str(trace), "--process-csv", str(procs), "--quantum", "2")
        assert row["makespan"] == 6
        trace_rows = list(csv.DictReader(trace.open()))
        proc_rows = list(csv.DictReader(procs.open()))
        assert trace_rows
        assert len(proc_rows) == 3
        assert sum(int(r["duration"]) for r in trace_rows) == 6
        assert {r["pid"] for r in proc_rows} == {"A", "B", "C"}


def main():
    tests = [
        test_fcfs_known_metrics,
        test_all_algorithms_keep_core_invariants,
        test_mlfq_improves_interactive_response_over_fcfs,
        test_qlearn_is_adaptive_on_interactive_workload,
        test_trace_and_process_exports,
    ]
    for test in tests:
        test()
        print(f"PASS {test.__name__}")


if __name__ == "__main__":
    main()
