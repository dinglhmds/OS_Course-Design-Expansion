#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

QUANTUM="${QUANTUM:-4}"
AGING="${AGING:-20}"
BOOST="${BOOST:-80}"
LEVELS="${LEVELS:-4}"
RL_ALPHA="${RL_ALPHA:-0.25}"
RL_GAMMA="${RL_GAMMA:-0.80}"
RL_EPSILON="${RL_EPSILON:-0.08}"

mkdir -p results
make all >/dev/null

RAW="results/raw.csv"
DETAIL_DIR="results/processes"
TRACE_DIR="results/traces"
mkdir -p "$DETAIL_DIR" "$TRACE_DIR"

echo "algorithm,workload,processes,makespan,avg_turnaround,avg_waiting,avg_response,avg_slowdown,throughput,context_switches,deadline_misses,cpu_utilization,fairness" > "$RAW"

algorithms="fcfs sjf srtf rr priority mlfq aamlfq qlearn"
for workload in workloads/*.csv; do
    name="$(basename "$workload" .csv)"
    for algo in $algorithms; do
        ./scheduler \
            --algo "$algo" \
            --input "$workload" \
            --workload "$name" \
            --quantum "$QUANTUM" \
            --aging "$AGING" \
            --boost "$BOOST" \
            --levels "$LEVELS" \
            --rl-alpha "$RL_ALPHA" \
            --rl-gamma "$RL_GAMMA" \
            --rl-epsilon "$RL_EPSILON" \
            --trace "$TRACE_DIR/${name}_${algo}.csv" \
            --process-csv "$DETAIL_DIR/${name}_${algo}.csv" \
            --csv >> "$RAW"
    done
done

python3 scripts/summarize.py "$RAW" > results/report.txt
python3 scripts/plot.py "$RAW" figures
echo "wrote $RAW"
echo "wrote results/report.txt"
echo "wrote results/charts.md"
echo "wrote figures/*.svg"
