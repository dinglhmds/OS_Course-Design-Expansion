from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd


ROOT = Path(__file__).resolve().parents[1]
RESULTS = ROOT / "results"
FIGURES = ROOT / "figures"
RAW_CSV = RESULTS / "raw.csv"
SUMMARY_CSV = RESULTS / "summary.csv"


def load_data() -> pd.DataFrame:
    if not RAW_CSV.exists():
        raise FileNotFoundError(f"missing input file: {RAW_CSV}")
    df = pd.read_csv(RAW_CSV)
    required = {
        "mode",
        "threads",
        "total_iters",
        "repeat",
        "elapsed_sec",
        "ns_per_op",
        "throughput",
        "final_counter",
        "correct",
    }
    missing = required.difference(df.columns)
    if missing:
        raise ValueError(f"raw.csv is missing columns: {sorted(missing)}")
    return df


def write_summary(df: pd.DataFrame) -> pd.DataFrame:
    summary = (
        df.groupby(["mode", "threads"], as_index=False)
        .agg(
            ns_per_op_mean=("ns_per_op", "mean"),
            ns_per_op_std=("ns_per_op", "std"),
            throughput_mean=("throughput", "mean"),
            throughput_std=("throughput", "std"),
        )
        .sort_values(["mode", "threads"])
    )
    summary.to_csv(SUMMARY_CSV, index=False)
    return summary


def plot_metric(summary: pd.DataFrame, metric: str, ylabel: str, output: Path) -> None:
    plt.figure(figsize=(9, 5.5))
    modes = ["race", "mutex", "spin", "atomic", "local"]
    for mode in modes:
        data = summary[summary["mode"] == mode].sort_values("threads")
        if data.empty:
            continue
        mean_col = f"{metric}_mean"
        std_col = f"{metric}_std"
        plt.errorbar(
            data["threads"],
            data[mean_col],
            yerr=data[std_col].fillna(0.0),
            marker="o",
            capsize=4,
            linewidth=1.8,
            label=mode,
        )

    plt.xlabel("Threads")
    plt.ylabel(ylabel)
    plt.xticks(sorted(summary["threads"].unique()))
    plt.grid(True, linestyle="--", linewidth=0.6, alpha=0.5)
    plt.legend(title="Mode")
    plt.tight_layout()
    plt.savefig(output, dpi=160)
    plt.close()


def main() -> None:
    RESULTS.mkdir(parents=True, exist_ok=True)
    FIGURES.mkdir(parents=True, exist_ok=True)
    df = load_data()
    summary = write_summary(df)
    plot_metric(summary, "ns_per_op", "Nanoseconds per operation", FIGURES / "ns_per_op.png")
    plot_metric(summary, "throughput", "Operations per second", FIGURES / "throughput.png")
    print(f"wrote {SUMMARY_CSV}")
    print(f"wrote {FIGURES / 'ns_per_op.png'}")
    print(f"wrote {FIGURES / 'throughput.png'}")


if __name__ == "__main__":
    main()
