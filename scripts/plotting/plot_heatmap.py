import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
import matplotlib.colors as colors
from matplotlib.ticker import FuncFormatter, MaxNLocator
import os
import numpy as np

# ==========================================
# CONFIGURATION - Adjust these for your figure
# ==========================================
# Target file path (e.g., "block_test_aurora_times.csv" or "block_test_veloc_times.csv")
TESTS_VERSION = "0.0.2-3"
RESULTS_DIR = f"./results/{TESTS_VERSION}"
INPUT_FILE = f"{RESULTS_DIR}/block_test_aurora_times.csv"

# CHOOSE YOUR METRIC:
# Options in this file: "mem_protect", "wait_timer", "ckpt_app_block", "mem_unprotect", "ckpt_total", "restart"
TARGET_METRIC = "ckpt_total"

# SCALE OPTION: "linear" or "log"
PLOT_SCALE = "linear"

# DATA FILTERING (Matches the raw numbers directly now)
TARGET_PROC_COUNT = 128     # Options in file: 64, 128
MEM_FILTER_MB = 1024      # Options in file: 1024, 4096, 16384, 32768
MAX_RANKS = 128             # Truncate view up to a certain rank

# Setup dynamic directory and save path
OUTPUT_IMAGE = f"{
    RESULTS_DIR}/heatmap_{TARGET_METRIC}_{PLOT_SCALE}_p{TARGET_PROC_COUNT}.png"
# ==========================================


def generate_custom_heatmap():
    if not os.path.exists(INPUT_FILE):
        print(f"Error: Target file '{INPUT_FILE}' does not exist.")
        return

    # Load file
    master_df = pd.read_csv(INPUT_FILE)

    # 1. Map directly to the column (no math adjustment needed since numbers match)
    plot_df = master_df[
        (master_df['metric'] == TARGET_METRIC) &
        (master_df['mem_mb'] == MEM_FILTER_MB) &
        (master_df['proc_count'] == TARGET_PROC_COUNT) &
        (master_df['rank'] < MAX_RANKS)
    ].copy()

    if plot_df.empty:
        print(f"Warning: No rows matched your filter choices.\n"
              f"Check that metric={TARGET_METRIC}, mem_kb column value={MEM_FILTER_MB} MB, and proc_count={TARGET_PROC_COUNT} exist together.")
        return

    # 2. Aggregate and Pivot (Rows: server_threads, Columns: rank)
    pivot_ready = plot_df.groupby(['server_threads', 'rank'])[
        'time_ms'].mean().reset_index()

    # Ensure rank is treated cleanly as integer columns
    pivot_ready['rank'] = pivot_ready['rank'].astype(int)

    heatmap_data = pivot_ready.pivot(
        index="server_threads", columns="rank", values="time_ms")
    heatmap_data = heatmap_data.sort_index(ascending=True)

    plt.figure(figsize=(16, 6))

    # 3. Setup Scale Normalization
    vmin, vmax = heatmap_data.min().min(), heatmap_data.max().max()
    norm = colors.PowerNorm(gamma=0.3, vmin=vmin,
                            vmax=vmax) if PLOT_SCALE == "log" else None

    # Formatter for clean colorbar annotations
    def format_func(value, tick_number):
        if value >= 1000:
            return f'{value/1000:.2f}k'
        return f'{value:.2f}'

    # 4. Create Heatmap via imshow
    im = plt.imshow(
        heatmap_data,
        aspect='auto',
        cmap="magma",
        norm=norm,
        interpolation=None  # Preserves discrete per-rank cell boxes
    )

    # Colorbar layout customization
    cbar = plt.colorbar(im)
    cbar.set_label('Time (ms)', fontsize=12)
    cbar.ax.yaxis.set_major_locator(MaxNLocator(nbins=12))
    cbar.ax.yaxis.set_major_formatter(FuncFormatter(format_func))

    # Explicitly map indices back to the source values
    plt.yticks(range(len(heatmap_data.index)), heatmap_data.index)

    # Label formatting with accurate MB sizes (e.g., 32768 MB)
    plt.title(f"Aurora {TARGET_METRIC} Latency per Rank\n"
              f"({MEM_FILTER_MB} MB Checkpoint Size | MPI Procs: {TARGET_PROC_COUNT})",
              fontsize=15, pad=15)
    plt.xlabel("MPI Rank", fontsize=12)
    plt.ylabel("Server Thread Count", fontsize=12)

    # Step standard X-axis tick intervals so it doesn't get cluttered
    tick_step = 4 if MAX_RANKS <= 64 else 8
    plt.xticks(np.arange(0, len(heatmap_data.columns), tick_step),
               heatmap_data.columns[::tick_step])

    plt.tight_layout()
    plt.savefig(OUTPUT_IMAGE, dpi=300)
    print(f"Generated {OUTPUT_IMAGE}")


if __name__ == "__main__":
    generate_custom_heatmap()
