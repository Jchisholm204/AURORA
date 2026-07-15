import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
import numpy as np

# ==========================================
# CONFIGURATION
# ==========================================

# Input Benchmark files (e.g., baseline vs. optimized)
TESTS_VERSION = "0.0.2-1"
RESULTS_DIR = f"./results/{TESTS_VERSION}"
INPUT_CSV_1 = f"{RESULTS_DIR}/heatdis_aurora_final_times.csv"
INPUT_CSV_2 = f"{RESULTS_DIR}/heatdis_veloc_final_times.csv"

LABEL_CSV_1 = "AURORA"
LABEL_CSV_2 = "VELOC"

# Filters for the plot
TARGET_MPI_PROCS = 128
TARGET_BACKEND_PROCS = 16

OUTPUT_IMAGE = f"{
    RESULTS_DIR}/mpi{TARGET_MPI_PROCS}_backend{TARGET_BACKEND_PROCS}_runtime_vs_mem.png"

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
    Loads, filters for target processes, cleans outliers, 
    and averages the runtime across all trials per Memory size.
    """
    try:
        df = pd.read_csv(filepath)
    except FileNotFoundError:
        print(f"Warning: Could not find {filepath}. Skipping from plot.")
        return None

    # 1. Dynamically build the filtering query
    query_mask = pd.Series(True, index=df.index)

    if TARGET_MPI_PROCS is not None and 'MPI_Processes' in df.columns:
        query_mask &= (df['MPI_Processes'] == TARGET_MPI_PROCS)

    # Only apply backend filter if the column has actual, non-NaN values to filter by
    if TARGET_BACKEND_PROCS is not None and 'Backend_Processes' in df.columns:
        # Check if the dataset has any valid non-NaN integers in Backend_Processes
        if not df['Backend_Processes'].isna().all():
            query_mask &= (df['Backend_Processes'] == TARGET_BACKEND_PROCS)

    filtered_df = df[query_mask].copy()

    if filtered_df.empty:
        print(f"Warning: No matching data found in {
              filepath} for current filters.")
        return None

    # 2. Filter out invalid negative/zero runtimes
    filtered_df = filtered_df[filtered_df['Total_Runtime_Sec'] > 0]

    # 3. Clean outliers across trials of the same configuration
    # Group by columns that are actually present AND have non-null variables
    # We drop any grouping column that contains only NaN values (like Backend_Processes in VeloC)
    group_cols = ['Memory_MB']
    for col in ['MPI_Processes', 'Backend_Processes']:
        if col in filtered_df.columns and not filtered_df[col].isna().all():
            group_cols.append(col)

    filtered_df = filter_outliers(filtered_df, group_cols, 'Total_Runtime_Sec')

    # 4. Group by Memory_MB and average across trials
    averaged_df = filtered_df.groupby('Memory_MB', as_index=False)[
        'Total_Runtime_Sec'].mean()

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
    # - x: Memory_MB
    # - y: Total_Runtime_Sec (already averaged)
    # - hue: Source (to distinguish CSV 1 vs. CSV 2)
    ax = sns.lineplot(
        data=plot_df,
        x="Memory_MB",
        y="Total_Runtime_Sec",
        hue="Source",
        marker="o",
        linewidth=2,
        markersize=8,
        palette="Set1"
    )

    # Adjust limits and scale (using log base 2 for memory step intervals if typical)
    ax.set_xscale('log', base=2)

    # Customizing layout and labels
    plt.title(
        f"Total Runtime vs. Memory Size\n"
        f"(MPI Processes: {TARGET_MPI_PROCS} | Backend Processes: {
            TARGET_BACKEND_PROCS})",
        fontsize=14,
        pad=15
    )
    plt.xlabel("Memory (MB)", fontsize=11)
    plt.ylabel("Average Total Runtime (Seconds)", fontsize=11)

    # Format x-axis with standard scalar labels (so it shows 64, 128, etc. instead of 2^6)
    from matplotlib.ticker import ScalarFormatter
    ax.get_xaxis().set_major_formatter(ScalarFormatter())

    plt.legend(title="Benchmark Source", loc='best')
    plt.tight_layout()

    # Save output
    import os
    os.makedirs(os.path.dirname(OUTPUT_IMAGE), exist_ok=True)
    plt.savefig(OUTPUT_IMAGE, dpi=300)
    print(f"Comparison plot successfully saved to: {OUTPUT_IMAGE}")


if __name__ == "__main__":
    generate_runtime_comparison_plot()
