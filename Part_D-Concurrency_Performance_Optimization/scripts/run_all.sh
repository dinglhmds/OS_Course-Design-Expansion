#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

TOTAL_ITERS="${TOTAL_ITERS:-10000000}"
REPEAT="${REPEAT:-5}"
MODES="race mutex spin atomic local"
THREADS="1 2 4 8 16"

make
mkdir -p results

out="results/raw.csv"
printf "mode,threads,total_iters,repeat,elapsed_sec,ns_per_op,throughput,final_counter,correct\n" > "$out"

for mode in $MODES; do
    for threads in $THREADS; do
        ./bench_counter --mode "$mode" --threads "$threads" --iters "$TOTAL_ITERS" --repeat "$REPEAT" >> "$out"
    done
done

echo "wrote $out"
