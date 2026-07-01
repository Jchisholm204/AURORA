import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.ticker import ScalarFormatter

# ==========================================
# CONFIGURATION
# ==========================================
SERVER_THREADS = "16"

INPUT_CSV = f"./aurora{SERVER_THREADS}_bench_results.csv"
# INPUT_CSV = "./aurora16_bench_results.csv"
TARGET_METRIC = "restart"
PROC_COUNTS = [16, 32, 64]
Y_PEAK = 40000  # Manually cap the Y-axis for better visibility
# OUTPUT_IMAGE = f"scaling_clean_{TARGET_METRIC}.png"
OUTPUT_IMAGE = f"./images/{TARGET_METRIC}s/{SERVER_THREADS}_metrics.png"
# ==========================================


def filter_outliers(df, group_cols, target_col):
    """Removes outliers using the IQR method (1.5 * IQR) for each group."""
    def remove_group_outliers(group):
        q1 = group[target_col].quantile(0.25)
        q3 = group[target_col].quantile(0.75)
        iqr = q3 - q1
        lower_bound = q1 - 1.5 * iqr
        upper_bound = q3 + 1.5 * iqr
        return group[(group[target_col] >= lower_bound) & (group[target_col] <= upper_bound)]

    return df.groupby(group_cols, group_keys=False).apply(remove_group_outliers)


def generate_scaling_plot():
    # 1. Load data
    df = pd.read_csv(INPUT_CSV)

    # 2. Initial Filter
    plot_df = df[
        (df['metric'] == TARGET_METRIC) &
        (df['proc_count'].isin(PROC_COUNTS))
    ].copy()

    if plot_df.empty:
        print("No data found.")
        return

    # 3. STATISTICAL CLEANING
    # Remove negative values first
    plot_df = plot_df[plot_df['time_ms'] > 0]

    # Remove outliers per (memory_size, process_count) group
    # This prevents one bad run from ruining the average of the other 4
    plot_df = filter_outliers(plot_df, ['mem_kb', 'proc_count'], 'time_ms')

    plt.figure(figsize=(10, 7))
    sns.set_theme(style="whitegrid")

    # 4. Power Scale Functions (for stretching 0-100ms range)
    def forward(x):
        # Gamma 0.5 is a square root scale, very readable
        return np.power(x, 0.5)

    def inverse(x):
        return np.power(x, 1/0.5)

    # 5. Plotting
    # Note: Using 'sd' (Standard Deviation) now that outliers are gone
    ax = sns.lineplot(
        data=plot_df,
        x="mem_kb",
        y="time_ms",
        hue="proc_count",
        marker="o",
        palette="viridis",
        errorbar="sd"
    )

    # 6. Apply Scaling
    ax.set_xscale('log', base=2)
    ax.set_yscale('function', functions=(forward, inverse))

    # 7. Strictly Limit Y-Axis to show the 0-400ms range
    ax.set_ylim(0, Y_PEAK)

    # 8. Y-Axis Ticks (Granular in the low range)
    y_ticks = [0, 25, 50, 100, 150, 200, 300, 400]
    ax.set_yticks(y_ticks)
    ax.get_yaxis().set_major_formatter(ScalarFormatter())

    plt.title(f"Aurora Scaling: {TARGET_METRIC} (Outliers Removed)\nFocused on 0-{Y_PEAK}ms Range",
              fontsize=14, pad=15)
    plt.xlabel("Total Memory Size (KB)", fontsize=12)
    plt.ylabel("Latency (ms)", fontsize=12)
    plt.legend(title="MPI Processes")

    plt.tight_layout()
    plt.savefig(OUTPUT_IMAGE, dpi=300)
    print(f"Saved clean scaling plot to {OUTPUT_IMAGE}")


if __name__ == "__main__":
    generate_scaling_plot()
