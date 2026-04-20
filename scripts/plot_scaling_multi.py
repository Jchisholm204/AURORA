import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.ticker import ScalarFormatter

# ==========================================
# CONFIGURATION
# ==========================================

SERVER_THREADS = "32"

INPUT_CSV = f"./aurora{SERVER_THREADS}_bench_results.csv"
# Assuming 'mem_unprotect' is your Start metric
METRICS = ["restart", "ckpt_total"]
# METRICS = ["ckpt_start", "ckpt_total"]
PROC_COUNTS = [16, 32]
Y_PEAK = 10000  # Adjusted slightly for the dual metrics

OUTPUT_IMAGE = f"./images/{SERVER_THREADS}_cr_metrics.png"
# ==========================================


def filter_outliers(df, group_cols, target_col):
    def remove_group_outliers(group):
        q1 = group[target_col].quantile(0.25)
        q3 = group[target_col].quantile(0.75)
        iqr = q3 - q1
        return group[(group[target_col] >= (q1 - 1.5 * iqr)) & (group[target_col] <= (q3 + 1.5 * iqr))]
    return df.groupby(group_cols, group_keys=False).apply(remove_group_outliers)


def generate_dual_scaling_plot():
    df = pd.read_csv(INPUT_CSV)

    # 1. Filter for both metrics
    plot_df = df[
        (df['metric'].isin(METRICS)) &
        (df['proc_count'].isin(PROC_COUNTS))
    ].copy()

    # 2. Statistical Cleaning
    plot_df = plot_df[plot_df['time_ms'] > 0]
    plot_df = filter_outliers(
        plot_df, ['mem_kb', 'proc_count', 'metric'], 'time_ms')

    plt.figure(figsize=(12, 8))
    sns.set_theme(style="whitegrid")

    # 3. Power Scale Functions (Gamma 0.4 for good mid-range detail)
    def forward(x): return np.power(x, 0.4)
    def inverse(x): return np.power(x, 1/0.4)

    # 4. Create the Line Plot
    # - hue='proc_count': Colors for 16, 32, 64
    # - style='metric': Solid vs Dashed for Total vs Start
    ax = sns.lineplot(
        data=plot_df,
        x="mem_kb",
        y="time_ms",
        hue="proc_count",
        style="metric",
        markers=True,
        palette="viridis",
        # errorbar="sd"
        # estimator=np.min,  # Change from default mean to min
        errorbar=None      # Error bars for min values are usually omitted
    )

    # 5. Apply Scales and Limits
    ax.set_xscale('log', base=2)
    ax.set_yscale('function', functions=(forward, inverse))
    ax.set_ylim(1, Y_PEAK)

    # 6. Formatting Ticks
    y_ticks = [1, 50, 100, 200, 300, 400, 500]
    ax.set_yticks(y_ticks)
    ax.get_yaxis().set_major_formatter(ScalarFormatter())

    # 7. Labels
    plt.title(f"Aurora Checkpoint Scaling: Total vs. Start Time",
              fontsize=16, pad=20)
    plt.xlabel("Total Memory Size (KB)", fontsize=13)
    plt.ylabel("Latency (ms)", fontsize=13)

    # Clean up legend
    plt.legend(title="Process Count & Metric",
               bbox_to_anchor=(1.05, 1), loc='upper left')

    plt.tight_layout()
    plt.savefig(OUTPUT_IMAGE, dpi=300)
    print(f"Dual-metric scaling plot saved: {OUTPUT_IMAGE}")


if __name__ == "__main__":
    generate_dual_scaling_plot()
