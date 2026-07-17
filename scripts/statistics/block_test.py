import pandas as pd
import os

# ==========================================
# CONFIGURATION - Mirrors your heatmap script
# ==========================================
TESTS_VERSION = "0.0.2-3"
RESULTS_DIR = f"./results/{TESTS_VERSION}"
INPUT_FILE = f"{RESULTS_DIR}/block_test_aurora_times.csv"

# CHOOSE YOUR METRIC:
TARGET_METRIC = "ckpt_total"

# DATA FILTERING
TARGET_PROC_COUNT = 128     # Options: 64, 128
MEM_FILTER_MB = 1024       # Options: 1024, 4096, 16384, 32768
MAX_RANKS = 128            # Filter up to a certain rank
# ==========================================


def generate_statistical_summary():
    if not os.path.exists(INPUT_FILE):
        print(f"Error: Target file '{INPUT_FILE}' does not exist.")
        return

    # Load file
    master_df = pd.read_csv(INPUT_FILE)

    # Detect whether column is named 'mem_mb' or 'mem_kb' to match your runtime dataset
    mem_col = 'mem_mb' if 'mem_mb' in master_df.columns else 'mem_kb'

    # Apply your exact script filtering logic
    plot_df = master_df[
        (master_df['metric'] == TARGET_METRIC) &
        (master_df[mem_col] == MEM_FILTER_MB) &
        (master_df['proc_count'] == TARGET_PROC_COUNT) &
        (master_df['rank'] < MAX_RANKS)
    ].copy()

    if plot_df.empty:
        print(f"Warning: No rows matched your filter choices.\n"
              f"Check that metric={TARGET_METRIC}, {mem_col}={MEM_FILTER_MB}, and proc_count={TARGET_PROC_COUNT} match data.")
        return

    print("=" * 70)
    print(f"STATISTICAL SUMMARY FOR: {TARGET_METRIC}")
    print(f"Configuration: {MEM_FILTER_MB} MB Checkpoint | {
          TARGET_PROC_COUNT} MPI Procs (< Rank {MAX_RANKS})")
    print("=" * 70)

    # --- 1. OVERALL STATS ACROSS ALL TRANSACTIONS ---
    print("\n[1] Overall Aggregated Statistics (All Ranks & Server Threads Combined):")
    overall = plot_df['time_ms'].describe(percentiles=[.25, .5, .75])
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

    # Custom aggregation function to easily pull specific metrics out of groupby
    summary_stats = plot_df.groupby('server_threads')['time_ms'].agg(
        Count='count',
        Mean='mean',
        StdDev='std',
        Min='min',
        Q1=lambda x: x.quantile(0.25),
        Median='median',
        Q3=lambda x: x.quantile(0.75),
        Max='max'
    )

    # Display the neat table
    print(summary_stats.to_string(formatters={
        'Count': '{:,.0f}'.format,
        'Mean': '{:,.2f} ms'.format,
        'StdDev': '{:,.2f} ms'.format,
        'Min': '{:,.2f} ms'.format,
        'Q1': '{:,.2f} ms'.format,
        'Median': '{:,.2f} ms'.format,
        'Q3': '{:,.2f} ms'.format,
        'Max': '{:,.2f} ms'.format
    }))

    # --- 3. IDENTIFY TOP OUTLIERS/HIGH LATENCY RANKS ---
    print("\n[3] Top 5 Highest Latency Ranks (Averaged across runs):")
    rank_breakdown = plot_df.groupby(['server_threads', 'rank'])[
        'time_ms'].mean().reset_index()
    top_stragglers = rank_breakdown.sort_values(
        by='time_ms', ascending=False).head(5)

    for idx, row in top_stragglers.iterrows():
        print(f"  Server Threads: {int(row['server_threads']):>2} | Rank: {
              int(row['rank']):>3} | Avg Latency: {row['time_ms']:.3f} ms")
    print("=" * 70)


if __name__ == "__main__":
    generate_statistical_summary()
