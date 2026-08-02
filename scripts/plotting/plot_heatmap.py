import os
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.colors as colors
from matplotlib.ticker import FuncFormatter, MaxNLocator

# ==========================================
# CONFIGURATION - Adjust parameters here
# ==========================================
TESTS_VERSION = "0.0.3"
RESULTS_DIR = f"./results/{TESTS_VERSION}"
INPUT_FILE = f"{RESULTS_DIR}/block_test_aurora_heatmap_times.csv"
OUTPUT_DIR = f"{RESULTS_DIR}/heatmaps"

# List all metrics you want to batch export
TARGET_METRICS = [
    # "mem_protect",
    # "wait_timer",
    "ckpt_app_block",
    # "mem_unprotect",
    "ckpt_total",
    "restart"
]

# SCALE OPTION: "linear" or "log"
PLOT_SCALE = "linear"

# DATA FILTERING
TARGET_PROC_COUNT = 128    # Options: 64, 128
MEM_FILTER_MB = 2048       # Options: 1024, 4096, 16384, 32768
MAX_RANKS = 128            # Max ranks to include

# VISUAL CONFIGURATION (Paper-Ready Sizing)
FONT_SIZE_AXIS_LABELS = 16
FONT_SIZE_TICKS = 14
FONT_SIZE_CBAR = 14
FIG_SIZE = (8, 4)         # Width, Height in inches suitable for paper columns
DPI = 300
# ==========================================


def apply_publication_styles():
    """Sets matplotlib default settings for high-legibility figure formats."""
    plt.rcParams.update({
        'font.size': FONT_SIZE_TICKS,
        'axes.labelsize': FONT_SIZE_AXIS_LABELS,
        'axes.titlesize': FONT_SIZE_AXIS_LABELS,
        'xtick.labelsize': FONT_SIZE_TICKS,
        'ytick.labelsize': FONT_SIZE_TICKS,
        'figure.autolayout': False
    })


def generate_custom_heatmap(metric, master_df):
    # Filter data for current metric
    plot_df = master_df[
        (master_df['metric'] == metric) &
        (master_df['mem_mb'] == MEM_FILTER_MB) &
        (master_df['proc_count'] == TARGET_PROC_COUNT) &
        (master_df['rank'] < MAX_RANKS)
    ].copy()

    if plot_df.empty:
        print(f"Warning: No rows matched filter choices for metric: '{
              metric}'. Skipping.")
        return

    # Aggregate and Pivot (Rows: server_threads, Columns: rank)
    pivot_ready = plot_df.groupby(['server_threads', 'rank'])[
        'time_ms'].mean().reset_index()
    pivot_ready['rank'] = pivot_ready['rank'].astype(int)

    heatmap_data = pivot_ready.pivot(
        index="server_threads", columns="rank", values="time_ms")
    heatmap_data = heatmap_data.sort_index(ascending=True)

    # Sort values per row (across ranks) from smallest to largest to produce gradient
    sorted_values = np.sort(heatmap_data.values, axis=1)

    # Setup Plot
    fig, ax = plt.subplots(figsize=FIG_SIZE, layout="constrained")

    # Setup Scale Normalization
    vmin, vmax = np.nanmin(sorted_values), np.nanmax(sorted_values)
    norm = colors.PowerNorm(gamma=0.3, vmin=vmin,
                            vmax=vmax) if PLOT_SCALE == "log" else None

    # Formatter for colorbar ticks
    def format_func(value, tick_number):
        if value >= 1000:
            return f'{value/1000:.1f}k'
        return f'{value:.1f}'

    # Create Heatmap
    im = ax.imshow(
        sorted_values,
        aspect='auto',
        cmap="magma",
        norm=norm,
        interpolation=None
    )

    # Colorbar configuration
    cbar = fig.colorbar(im, ax=ax)
    cbar.set_label('Time (ms)', fontsize=FONT_SIZE_AXIS_LABELS)
    cbar.ax.tick_params(labelsize=FONT_SIZE_CBAR)
    cbar.ax.yaxis.set_major_locator(MaxNLocator(nbins=6))
    cbar.ax.yaxis.set_major_formatter(FuncFormatter(format_func))

    # Y-axis Ticks (Server Threads)
    ax.set_yticks(range(len(heatmap_data.index)))
    ax.set_yticklabels(heatmap_data.index)
    ax.set_ylabel("Server Thread Count")

    # X-axis Ticks (Powers of 2 fractions: 0, 1/4, 1/2, 3/4, 1/1)
    max_idx = sorted_values.shape[1] - 1
    x_positions = [
        0,
        int(round(max_idx * 0.25)),
        int(round(max_idx * 0.50)),
        int(round(max_idx * 0.75)),
        max_idx
    ]

    # Format tick labels based on powers-of-2 scaling
    x_labels = [str(p) for p in x_positions]

    ax.set_xticks(x_positions)
    ax.set_xticklabels(x_labels)
    ax.set_xlabel("Sorted MPI Ranks")

    # Save output image
    output_filename = f"heatmap_{metric}_{PLOT_SCALE}_p{
        TARGET_PROC_COUNT}_{MEM_FILTER_MB}MB.png"
    output_path = os.path.join(OUTPUT_DIR, output_filename)

    plt.savefig(output_path, dpi=DPI)
    plt.close(fig)
    print(f"Successfully generated: {output_path}")


def main():
    apply_publication_styles()

    if not os.path.exists(INPUT_FILE):
        print(f"Error: Target file '{INPUT_FILE}' does not exist.")
        return

    # Create output directory if needed
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    # Load data once
    master_df = pd.read_csv(INPUT_FILE)

    # Run for each requested metric
    for metric in TARGET_METRICS:
        generate_custom_heatmap(metric, master_df)


if __name__ == "__main__":
    main()
