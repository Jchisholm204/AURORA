import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
import numpy as np
import os
from matplotlib.ticker import ScalarFormatter

# ==========================================
# CONFIGURATION
# ==========================================
TESTS_VERSION = "0.0.2-3"
RESULTS_DIR = f"./results/{TESTS_VERSION}"

# Input block test files
INPUT_CSV_1 = f"{RESULTS_DIR}/block_test_aurora_times.csv"
INPUT_CSV_2 = f"{RESULTS_DIR}/block_test_veloc_times.csv"

LABEL_CSV_1 = "AURORA"
LABEL_CSV_2 = "VELOC"

# 1. CHOOSE YOUR TARGET METRIC
# Options in these files: "ckpt_total", "ckpt_app_block", "mem_protect", "mem_unprotect", "restart"
TARGET_METRIC = "ckpt_app_block"

# 2. DATA FILTERING OPTIONS
PROC_COUNTS = [64, 128]

# Target server threads for Aurora (safely ignored for VeloC's native -1 thread mapping)
TARGET_SERVER_THREADS = 16

# Create a clean output filename
metric_slug = TARGET_METRIC.lower().replace(" ", "_")
OUTPUT_IMAGE = f"{
    RESULTS_DIR}/block_test_comparison_{metric_slug}_t{TARGET_SERVER_THREADS}.png"
# ==========================================


def filter_outliers(df, group_cols, target_col):
    """
    Removes statistical outliers per group based on the IQR method.
    Uses transform to preserve the DataFrame's column structure.
    """
    g = df.groupby(group_cols)[target_col]
    q1 = g.transform('quantile', 0.25)
    q3 = g.transform('quantile', 0.75)
    iqr = q3 - q1

    mask = (df[target_col] >= (q1 - 1.5 * iqr)
            ) & (df[target_col] <= (q3 + 1.5 * iqr))
    return df[mask]


def load_and_filter_dataset(filepath, label):
    """
    Loads block test files, filters by metric and active process layout, 
    cleans outliers, and computes average time_ms per configuration.
    """
    try:
        df = pd.read_csv(filepath)
    except FileNotFoundError:
        print(f"Warning: Could not find {filepath}. Skipping from plot.")
        return None

    # 1. Filter for the target metric
    if 'metric' in df.columns:
        filtered_df = df[df['metric'] == TARGET_METRIC].copy()
    else:
        print(f"Error: 'metric' column missing in {filepath}")
        return None

    if filtered_df.empty:
        print(f"Warning: No matching rows found for metric '{
              TARGET_METRIC}' in {filepath}.")
        return None

    # 2. Dynamically filter process counts and active daemon threads
    query_mask = pd.Series(True, index=filtered_df.index)

    if 'proc_count' in filtered_df.columns:
        query_mask &= (filtered_df['proc_count'].isin(PROC_COUNTS))

    # Apply server thread filtering only if the dataset has varying configurations (e.g., Aurora)
    if 'server_threads' in filtered_df.columns and TARGET_SERVER_THREADS is not None:
        # If the file contains multiple valid worker configs, pin to target
        unique_threads = filtered_df['server_threads'].dropna().unique()
        if len(unique_threads) > 1 and TARGET_SERVER_THREADS in unique_threads:
            query_mask &= (filtered_df['server_threads']
                           == TARGET_SERVER_THREADS)

    filtered_df = filtered_df[query_mask].copy()

    if filtered_df.empty:
        print(f"Warning: No matching data found in {
              filepath} after applying filters.")
        return None

    # 3. Clean invalid timelines
    filtered_df = filtered_df[filtered_df['time_ms'] > 0]

    # 4. Clean outliers across matching execution subsets
    group_cols = ['mem_mb']
    for col in ['proc_count', 'server_threads']:
        if col in filtered_df.columns and not filtered_df[col].isna().all():
            group_cols.append(col)

    filtered_df = filter_outliers(filtered_df, group_cols, 'time_ms')

    # 5. Collapse across trials and ranks to get a single clear average metric line
    agg_cols = ['mem_mb', 'proc_count']
    averaged_df = filtered_df.groupby(agg_cols, as_index=False)[
        'time_ms'].mean()

    # Assign identity metadata
    averaged_df['Framework'] = label

    return averaged_df


def generate_combined_scaling_plot():
    # Load and process data structures
    df1 = load_and_filter_dataset(INPUT_CSV_1, LABEL_CSV_1)
    df2 = load_and_filter_dataset(INPUT_CSV_2, LABEL_CSV_2)

    datasets_to_plot = [df for df in [df1, df2] if df is not None]

    if not datasets_to_plot:
        print("Error: No valid data available to plot.")
        return

    plot_df = pd.concat(datasets_to_plot, ignore_index=True)
    plot_df['proc_count'] = plot_df['proc_count'].astype(str) + " Cores"

    # Set up layout styling
    plt.figure(figsize=(11, 7))
    sns.set_theme(style="whitegrid")

    # Multi-dimensional Line Plot
    # - hue="Framework": Color separates Aurora vs VeloC
    # - style="proc_count": Line style separates 64 vs 128 Cores
    ax = sns.lineplot(
        data=plot_df,
        x="mem_mb",
        y="time_ms",
        hue="Framework",
        style="proc_count",
        marker="o",
        linewidth=2.5,
        markersize=8,
        palette="Set1"
    )

    # Scale X-axis for log base-2 memory steps directly in MB (1024, 4096, etc.)
    ax.set_xscale('log', base=2)
    ax.get_xaxis().set_major_formatter(ScalarFormatter())

    # Labels & Title layout
    thread_info = f" (Aurora Server Threads: {
        TARGET_SERVER_THREADS})" if TARGET_SERVER_THREADS else ""
    plt.title(
        f"Block Test Scaling Profiles [64 & 128 Cores]\n"
        f"Metric: {TARGET_METRIC}{thread_info}",
        fontsize=14,
        pad=15
    )
    plt.xlabel("Memory Allocation Size (MB)", fontsize=12)
    plt.ylabel("Average Performance Latency (ms)", fontsize=12)

    plt.legend(title="Framework & Scale Topology", loc='best')
    plt.tight_layout()

    # Save output asset file
    os.makedirs(os.path.dirname(OUTPUT_IMAGE), exist_ok=True)
    plt.savefig(OUTPUT_IMAGE, dpi=300)
    print(f"Combined scaling comparison plot successfully saved to: {
          OUTPUT_IMAGE}")


if __name__ == "__main__":
    generate_combined_scaling_plot()
