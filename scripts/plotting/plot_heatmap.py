import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
import matplotlib.colors as colors
from matplotlib.ticker import ScalarFormatter, LogLocator, FuncFormatter
import os
import numpy as np

# ==========================================
# CONFIGURATION - Adjust these for your figure
# ==========================================
CSV_FILES = {
    8:  "./aurora8_bench_results.csv",
    16: "./aurora16_bench_results.csv",
    32: "./aurora32_bench_results.csv",
}

# CHOOSE YOUR METRIC:
# Options: "restart", "ckpt_total", "mem_unprotect" (from your JSON logs)
TARGET_METRIC = "ckpt_start"

# SCALE OPTION: "linear" or "log"
# Log scale is better for seeing detail in small values when large values exist
PLOT_SCALE = "linear"

MAX_RANKS = 32

# DATA FILTERING
# MEM_FILTER_KB = 16384  # 16MB
# MEM_FILTER_KB = 262144  # 256MB
# MEM_FILTER_KB = 1048576  # 1GB
# MEM_FILTER_KB = 16777216  # 16GB
MEM_FILTER_KB = 67108864  # 64GB
OUTPUT_IMAGE = f"./images/heatmaps_{MEM_FILTER_KB}/heatmap_{
    TARGET_METRIC}_{PLOT_SCALE}_{MEM_FILTER_KB}.png"
# ==========================================


def generate_custom_heatmap():
    all_dfs = []
    for threads, file_path in CSV_FILES.items():
        if os.path.exists(file_path):
            temp_df = pd.read_csv(file_path)
            temp_df['server_threads'] = threads
            all_dfs.append(temp_df)

    if not all_dfs:
        return
    master_df = pd.concat(all_dfs, ignore_index=True)
    plot_df = master_df[(master_df['metric'] == TARGET_METRIC) & (
        master_df['mem_kb'] == MEM_FILTER_KB)].copy()
    plot_df = plot_df[plot_df['rank'] < MAX_RANKS].copy()

    if plot_df.empty:
        return

    # Aggregate and Pivot
    pivot_ready = plot_df.groupby(['server_threads', 'rank'])[
        'time_ms'].mean().reset_index()
    heatmap_data = pivot_ready.pivot(
        index="server_threads", columns="rank", values="time_ms")
    heatmap_data = heatmap_data.sort_index(ascending=True)

    plt.figure(figsize=(16, 8))

    # 1. Use PowerNorm to stretch the lower values
    # gamma < 1 (e.g., 0.3) expands the lower end of the range visually.
    # gamma = 1 is linear.
    # gamma > 1 expands the higher end.
    vmin, vmax = heatmap_data.min().min(), heatmap_data.max().max()
    norm = None
    if PLOT_SCALE == "log":
        norm = colors.PowerNorm(gamma=0.3, vmin=vmin, vmax=vmax)
    else:
        norm = None

    # 2. Define a formatter to keep labels clean
    def format_func(value, tick_number):
        if value >= 1000:
            return f'{value/1000:.2f}k'
        return f'{value:.2f}'

# 3. Create Smoothed Heatmap
    # We use imshow for blending (interpolation)
    im = plt.imshow(
        heatmap_data,
        aspect='auto',
        cmap="magma",
        norm=norm,
        # interpolation='bilinear',  # This blends the colors
        # extent=[0, len(heatmap_data.columns)-0.5, 0, len(heatmap_data.index)-1]
        # extent=[0, len(heatmap_data.columns)-0.5, 0, len(heatmap_data.index)-1]
    )

    # Add the colorbar manually to control ticks
    cbar = plt.colorbar(im)
    cbar.set_label('Time (ms)', fontsize=12)

    # Increase the number of ticks on the colorbar
    # MaxNLocator(10) will try to find ~10 nice-looking intervals
    from matplotlib.ticker import MaxNLocator
    cbar.ax.yaxis.set_major_locator(MaxNLocator(nbins=24))
    cbar.ax.yaxis.set_major_formatter(FuncFormatter(format_func))

    # Re-align the Y-axis labels because imshow flips them compared to heatmap
    plt.yticks(range(len(heatmap_data.index)), heatmap_data.index)

    # 4. Final Formatting
    plt.title(f"Aurora {TARGET_METRIC} Latency per Rank, {MEM_FILTER_KB/1024}MB Checkpoint Size",
              fontsize=16, pad=20)

    plt.xlabel("MPI Rank", fontsize=12)
    plt.ylabel("Server Thread Count", fontsize=12)

    # Improve X-axis tick frequency
    plt.xticks(np.arange(0, len(heatmap_data.columns), 4),
               heatmap_data.columns[::4])

    plt.tight_layout()
    plt.savefig(OUTPUT_IMAGE, dpi=300)
    print(f"Generated {OUTPUT_IMAGE}")


if __name__ == "__main__":
    generate_custom_heatmap()
