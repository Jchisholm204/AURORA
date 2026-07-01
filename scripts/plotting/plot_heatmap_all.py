import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
import matplotlib.colors as colors
from matplotlib.ticker import FuncFormatter, FixedLocator
import os
import numpy as np

# Use a non-interactive backend to avoid GUI/Qt errors
import matplotlib
matplotlib.use('Agg')

# ==========================================
# CONFIGURATION
# ==========================================
CSV_FILES = {
    8:  "./aurora8_bench_results.csv",
    16: "./aurora16_bench_results.csv",
    32: "./aurora8_bench_results.csv",
}

TARGET_METRIC = "ckpt_total"
OUTPUT_IMAGE = f"multi_mem_heatmap_{TARGET_METRIC}.png"


def generate_custom_heatmap():
    print(f"Starting script... target metric: {TARGET_METRIC}")

    all_dfs = []
    for threads, file_path in CSV_FILES.items():
        if os.path.exists(file_path):
            print(f"Loading {file_path}...")
            temp_df = pd.read_csv(file_path)
            temp_df['server_threads'] = threads
            all_dfs.append(temp_df)
        else:
            print(f"Warning: File {file_path} not found.")

    if not all_dfs:
        print("Error: No data frames were loaded. Check your CSV file paths.")
        return

    master_df = pd.concat(all_dfs, ignore_index=True)
    plot_df = master_df[master_df['metric'] == TARGET_METRIC].copy()

    if plot_df.empty:
        print(f"Error: No data found for metric '{TARGET_METRIC}'.")
        return

    # 1. Setup FacetGrid
    g = sns.FacetGrid(plot_df, col="mem_kb", height=5, aspect=1.2, col_wrap=3)

    # Calculate global range for shared color scale
    vmin, vmax = plot_df['time_ms'].min(), plot_df['time_ms'].max()
    norm = colors.PowerNorm(gamma=0.3, vmin=vmin, vmax=vmax)

    def draw_heatmap(data, **kwargs):
        # Aggregate to solve "Duplicate Index" error
        pivoted = data.groupby(['server_threads', 'rank'])[
            'time_ms'].mean().unstack()
        pivoted = pivoted.sort_index(ascending=True)

        # Draw onto the current axes provided by FacetGrid
        sns.heatmap(pivoted, norm=norm, cmap="magma", cbar=False, linewidths=0)

    print("Mapping data to grid...")
    g.map_dataframe(draw_heatmap)

    # 2. Formatter
    def format_func(value, tick_number):
        if value >= 1000:
            return f'{value/1000:.1f}k'
        return f'{value:.0f}'

    # 3. Create Colorbar
    sm = plt.cm.ScalarMappable(cmap="magma", norm=norm)
    sm.set_array([])

    # Attach colorbar to the figure
    cbar = g.figure.colorbar(
        sm, ax=g.axes.ravel().tolist(), pad=0.04, aspect=30)
    cbar.set_label('Time (ms)', fontsize=12)

    # Explicit Low-End Ticks
    manual_ticks = [0, 10, 25, 50, 100, 250, 500, 1000, 2500, 5000, 10000]
    valid_ticks = [t for t in manual_ticks if t <= vmax]
    cbar.set_ticks(valid_ticks)
    cbar.ax.yaxis.set_major_formatter(FuncFormatter(format_func))

    # 4. Final adjustments
    g.set_axis_labels("MPI Rank", "Server Threads")
    g.set_titles("Memory: {col_name} KB")

    print(f"Saving image to {OUTPUT_IMAGE}...")
    plt.savefig(OUTPUT_IMAGE, dpi=300, bbox_inches='tight')
    print("Done.")


if __name__ == "__main__":
    generate_custom_heatmap()
