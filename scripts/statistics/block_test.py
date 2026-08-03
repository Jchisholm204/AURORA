import os
import pandas as pd

# ==========================================
# CONFIGURATION - Mirrors your heatmap script
# ==========================================
TESTS_VERSION = "0.0.3"
RESULTS_DIR = f"./results/{TESTS_VERSION}"
INPUT_FILE = f"{RESULTS_DIR}/block_test_aurora_heatmap_times.csv"
OUTPUT_DIR = f"{RESULTS_DIR}/block_stats"

# CHOOSE YOUR METRICS:
TARGET_METRICS = ["ckpt_total", "restart", "ckpt_app_block"]

# DATA FILTERING
TARGET_PROC_COUNT = 128  # Options: 64, 128
MEM_FILTER_MB = 16384  # Options: 1024, 4096, 16384, 32768
MAX_RANKS = 128  # Filter up to a certain rank

# OUTPUT OPTIONS
EXPORT_COMBINED_CSV = True  # Set to True to output one master CSV
EXPORT_PER_METRIC_CSV = True  # Set to True to output separate CSVs per metric
# ==========================================


def generate_statistical_summary():
    if not os.path.exists(INPUT_FILE):
        print(f"Error: Target file '{INPUT_FILE}' does not exist.")
        return

    # Load file
    master_df = pd.read_csv(INPUT_FILE)

    # Detect whether column is named 'mem_mb' or 'mem_kb'
    mem_col = "mem_mb" if "mem_mb" in master_df.columns else "mem_kb"

    all_summaries = []

    for target_metric in TARGET_METRICS:
        # Apply filtering logic
        plot_df = master_df[
            (master_df["metric"] == target_metric)
            & (master_df[mem_col] == MEM_FILTER_MB)
            & (master_df["proc_count"] == TARGET_PROC_COUNT)
            & (master_df["rank"] < MAX_RANKS)
        ].copy()

        if plot_df.empty:
            print(
                f"\nWarning: No rows matched filter choices for metric '{
                    target_metric}'.\n"
                f"Check that {mem_col}={MEM_FILTER_MB} and proc_count={
                    TARGET_PROC_COUNT} match data."
            )
            continue

        print("\n" + "=" * 70)
        print(f"STATISTICAL SUMMARY FOR: {target_metric}")
        print(
            f"Configuration: {MEM_FILTER_MB} MB Checkpoint | {
                TARGET_PROC_COUNT} MPI Procs (< Rank {MAX_RANKS})"
        )
        print("=" * 70)

        # --- 1. OVERALL STATS ACROSS ALL TRANSACTIONS ---
        print(
            "\n[1] Overall Aggregated Statistics (All Ranks & Server Threads Combined):"
        )
        overall = plot_df["time_ms"].describe(percentiles=[0.25, 0.5, 0.75])
        print(f"  Count:    {overall['count']:,.0f}")
        print(f"  Mean:     {overall['mean']:.3f} ms")
        print(f"  Std Dev:  {overall['std']:.3f} ms")
        print(f"  Min:      {overall['min']:.3f} ms")
        print(f"  Q1 (25%): {overall['25%']:.3f} ms")
        print(f"  Median:   {overall['50%']:.3f} ms")
        print(f"  Q3 (75%): {overall['75%']:.3f} ms")
        print(f"  Max:      {overall['max']:.3f} ms")

        # --- 2. BREAKDOWN BY SERVER THREAD COUNT ---
        print("\n[2] Breakdown Grouped by Server Thread Count:")

        summary_stats = (
            plot_df.groupby("server_threads")["time_ms"]
            .agg(
                Count="count",
                Mean="mean",
                StdDev="std",
                Min="min",
                Q1=lambda x: x.quantile(0.25),
                Median="median",
                Q3=lambda x: x.quantile(0.75),
                Max="max",
            )
            .reset_index()
        )

        # Print the console output formatted
        print(
            summary_stats.to_string(
                index=False,
                formatters={
                    "Count": "{:,.0f}".format,
                    "Mean": "{:,.2f} ms".format,
                    "StdDev": "{:,.2f} ms".format,
                    "Min": "{:,.2f} ms".format,
                    "Q1": "{:,.2f} ms".format,
                    "Median": "{:,.2f} ms".format,
                    "Q3": "{:,.2f} ms".format,
                    "Max": "{:,.2f} ms".format,
                },
            )
        )

        # --- 3. IDENTIFY TOP OUTLIERS/HIGH LATENCY RANKS ---
        print("\n[3] Top 5 Highest Latency Ranks (Averaged across runs):")
        rank_breakdown = (
            plot_df.groupby(["server_threads", "rank"])["time_ms"]
            .mean()
            .reset_index()
        )
        top_stragglers = rank_breakdown.sort_values(
            by="time_ms", ascending=False
        ).head(5)

        for idx, row in top_stragglers.iterrows():
            print(
                f"  Server Threads: {int(row['server_threads']):>2} | Rank: {
                    int(row['rank']):>3} | Avg Latency: {row['time_ms']:.3f} ms"
            )

        # Add metric column to the summary for output exports
        summary_stats.insert(0, "metric", target_metric)

        if not os.path.exists(f"{OUTPUT_DIR}"):
            os.mkdir(f"{OUTPUT_DIR}")

        # Optional: Export individual metric CSV
        if EXPORT_PER_METRIC_CSV:
            out_path = f"{OUTPUT_DIR}/summary_{target_metric}.csv"
            summary_stats.to_csv(out_path, index=False)
            print(f"\nSaved summary CSV to: {out_path}")

        all_summaries.append(summary_stats)

    # Export combined CSV if requested and data exists
    if EXPORT_COMBINED_CSV and all_summaries:
        combined_df = pd.concat(all_summaries, ignore_index=True)
        combined_out_path = f"{OUTPUT_DIR}/summary_all_metrics.csv"
        combined_df.to_csv(combined_out_path, index=False)
        print("\n" + "=" * 70)
        print(f"Master CSV saved to: {combined_out_path}")
        print("=" * 70)


if __name__ == "__main__":
    generate_statistical_summary()
