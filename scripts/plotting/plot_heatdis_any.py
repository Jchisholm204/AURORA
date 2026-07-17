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
INPUT_CSV_1 = f"{RESULTS_DIR}/heatdis_aurora_test_times.csv"
INPUT_CSV_2 = f"{RESULTS_DIR}/heatdis_veloc_test_times.csv"

LABEL_CSV_1 = "AURORA"
LABEL_CSV_2 = "VELOC"

# 1. Choose your Target Metric
# Common Options: "Total_Execution_Sec", "Checkpoint", "Memory Reg", "lib_Checkpoint Total"
TARGET_METRIC = "Checkpoint"

# 2. Filters for MPI and Backend configurations
TARGET_MPI_PROCS = 128
TARGET_BACKEND_PROCS = None  # Set to an integer (e.g., 16) or None to ignore

# Generate dynamic output name based on filters
backend_suffix = f"_backend{
    TARGET_BACKEND_PROCS}" if TARGET_BACKEND_PROCS is not None else ""
metric_clean_name = TARGET_METRIC.lower().replace(" ", "_")
OUTPUT_IMAGE = f"{
    RESULTS_DIR}/mpi{TARGET_MPI_PROCS}{backend_suffix}_{metric_clean_name}_vs_mem.png"

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
    Loads, filters for target metric, target processes, cleans outliers, 
    and averages the metric Time across all trials per Memory size.
    """
    try:
        df = pd.read_csv(filepath)
    except FileNotFoundError:
        print(f"Warning: Could not find {filepath}. Skipping from plot.")
        return None

    # 1. Filter for the Target Metric first
    if 'Metric' in df.columns:
        filtered_df = df[df['Metric'] == TARGET_METRIC].copy()
    else:
        print(f"Error: 'Metric' column not found in {filepath}.")
        return None

    if filtered_df.empty:
        print(f"Warning: No rows matched metric '{
              TARGET_METRIC}' in {filepath}.")
        return None

    # 2. Dynamically build the filtering query for MPI and Backend processes
    query_mask = pd.Series(True, index=filtered_df.index)

    if TARGET_MPI_PROCS is not None and 'MPI_Processes' in filtered_df.columns:
        query_mask &= (filtered_df['MPI_Processes'] == TARGET_MPI_PROCS)

    # Only apply backend filter if a target value is requested and the column isn't entirely NaN
    if TARGET_BACKEND_PROCS is not None and 'Backend_Processes' in filtered_df.columns:
        if not filtered_df['Backend_Processes'].isna().all():
            query_mask &= (
                filtered_df['Backend_Processes'] == TARGET_BACKEND_PROCS)

    filtered_df = filtered_df[query_mask].copy()

    if filtered_df.empty:
        print(f"Warning: No matching data found in {
              filepath} for current configuration filters.")
        return None

    # 3. Filter out invalid negative/zero times
    filtered_df = filtered_df[filtered_df['Time'] > 0]

    # 4. Clean outliers across trials of the same configuration
    group_cols = ['Memory_MB']
    for col in ['MPI_Processes', 'Backend_Processes']:
        if col in filtered_df.columns and not filtered_df[col].isna().all():
            group_cols.append(col)

    filtered_df = filter_outliers(filtered_df, group_cols, 'Time')

    # 5. Group by Memory_MB and average across trials
    averaged_df = filtered_df.groupby(
        'Memory_MB', as_index=False)['Time'].mean()

    # Label this dataset so we can distinguish them on the plot
    averaged_df['Source'] = label

    return averaged_df


def generate_runtime_comparison_plot():
    # Load and process both datasets
    df1 = load_and_filter_dataset(INPUT_CSV_1, LABEL_CSV_1)
    df2 = load_and_filter_dataset(INPUT_CSV_2, LABEL_CSV_2)

    # Combine valid datasets
    datasets_to_plot = [df for df in [df1, df2] if df is not None]

    if not datasets_to_plot:
        print("Error: No valid data available to plot.")
        return

    plot_df = pd.concat(datasets_to_plot, ignore_index=True)

    # Set up styling
    plt.figure(figsize=(10, 6))
    sns.set_theme(style="whitegrid")

    # Create the Line Plot
    ax = sns.lineplot(
        data=plot_df,
        x="Memory_MB",
        y="Time",
        hue="Source",
        marker="o",
        linewidth=2,
        markersize=8,
        palette="Set1"
    )

    ax.set_xscale('log', base=2)

    # Customizing title dynamically based on filters
    title_parts = [f"Metric: {TARGET_METRIC}", f"MPI: {TARGET_MPI_PROCS}"]
    if TARGET_BACKEND_PROCS is not None:
        title_parts.append(f"Backend: {TARGET_BACKEND_PROCS}")
    else:
        title_parts.append("All Backend Configs")

    plt.title(
        f"Performance Comparison vs. Memory Size\n" f"({
            ' | '.join(title_parts)})",
        fontsize=14,
        pad=15
    )
    plt.xlabel("Memory (MB)", fontsize=11)
    plt.ylabel("Average Time (Seconds / ms)", fontsize=11)

    ax.get_xaxis().set_major_formatter(ScalarFormatter())

    plt.legend(title="Benchmark Source", loc='best')
    plt.tight_layout()

    # Save output
    os.makedirs(os.path.dirname(OUTPUT_IMAGE), exist_ok=True)
    plt.savefig(OUTPUT_IMAGE, dpi=300)
    print(f"Comparison plot successfully saved to: {OUTPUT_IMAGE}")


if __name__ == "__main__":
    generate_runtime_comparison_plot()
