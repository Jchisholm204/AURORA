import os
import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
from matplotlib.ticker import ScalarFormatter

# ==========================================
# CONFIGURATION - Adjust parameters here
# ==========================================
TESTS_VERSION = "0.0.3"
RESULTS_DIR = f"./results/{TESTS_VERSION}"
OUTPUT_DIR = f"{RESULTS_DIR}/figures"

# 1. MULTI-CSV INPUT CONFIGURATION:
INPUT_DATASETS = [
    {"path": f"{RESULTS_DIR}/heatdis_aurora_restore_times.csv", "label": "AURORA"},
    {"path": f"{RESULTS_DIR}/heatdis_veloc_restore_times.csv", "label": "VELOC"},
]

# 2. TARGET THREAD FILTERING:
# Filter server threads when applicable (e.g., AURORA)
TARGET_SERVER_THREADS = 16

# 3. CORE COUNTS TO PLOT:
PROC_COUNTS = [64, 128]

# 4. ERROR BAR METRIC CONFIGURATION:
# Options: 'sd' (Standard Deviation across trials) or 'se' (Standard Error of Mean)
ERROR_TYPE = 'sd'

# 5. PUBLICATION SIZING & STYLING:
FIG_SIZE = (8, 5)
FONT_SIZE_AXIS_LABELS = 16
FONT_SIZE_TICKS = 14
FONT_SIZE_LEGEND = 12
DPI = 300
# ==========================================


def apply_publication_styles():
    """Sets global matplotlib parameters for publication-ready legibility."""
    plt.rcParams.update({
        'font.size': FONT_SIZE_TICKS,
        'axes.labelsize': FONT_SIZE_AXIS_LABELS,
        'xtick.labelsize': FONT_SIZE_TICKS,
        'ytick.labelsize': FONT_SIZE_TICKS,
        'legend.fontsize': FONT_SIZE_LEGEND,
        'figure.autolayout': False
    })


def normalize_dataframe_schema(df):
    """Standardizes dataset schema column names across CSV formats."""
    rename_dict = {}
    if 'MPI_Processes' in df.columns:
        rename_dict['MPI_Processes'] = 'proc_count'
    if 'Memory_MB' in df.columns:
        rename_dict['Memory_MB'] = 'mem_mb'
    if 'Backend_Processes' in df.columns:
        rename_dict['Backend_Processes'] = 'server_threads'

    return df.rename(columns=rename_dict)


def load_and_aggregate_runtime(dataset_info):
    """
    Loads CSV, filters threads (if present), and aggregates Total_Runtime_Sec
    across all trial iterations for each run configuration.
    """
    filepath = dataset_info["path"]
    label = dataset_info["label"]

    if not os.path.exists(filepath):
        print(f"Warning: File not found: {filepath}")
        return None

    try:
        df = pd.read_csv(filepath)
    except Exception as e:
        print(f"Error reading {filepath}: {e}")
        return None

    df = normalize_dataframe_schema(df)

    if 'Total_Runtime_Sec' not in df.columns:
        print(f"Warning: 'Total_Runtime_Sec' not found in {
              filepath}. Available cols: {list(df.columns)}")
        return None

    # Thread filtering (applied only if 'server_threads' contains valid thread counts)
    if 'server_threads' in df.columns and TARGET_SERVER_THREADS is not None:
        valid_threads = df['server_threads'].dropna().unique()
        if len(valid_threads) > 1 and TARGET_SERVER_THREADS in valid_threads:
            df = df[df['server_threads'] == TARGET_SERVER_THREADS]

    # Aggregate across trials for each configuration (proc_count x mem_mb)
    agg_df = df.groupby(['proc_count', 'mem_mb'], as_index=False).agg(
        time_mean=('Total_Runtime_Sec', 'mean'),
        time_std=('Total_Runtime_Sec', 'std'),
        time_sem=('Total_Runtime_Sec', lambda x: x.sem()
                  if len(x) > 1 else 0.0),
        trial_count=('Total_Runtime_Sec', 'count')
    )

    agg_df['time_err'] = agg_df['time_std'] if ERROR_TYPE == 'sd' else agg_df['time_sem']
    agg_df['Framework'] = label

    return agg_df


def render_plot_variant(data_df, core_mode_label):
    """Generates and exports an individual plot variant using aggregated trial runtimes."""
    if data_df.empty:
        return

    sns.set_theme(style="whitegrid")
    fig, ax = plt.subplots(figsize=FIG_SIZE, layout="constrained")

    apply_publication_styles()

    data_df['Cores_Label'] = data_df['proc_count'].astype(str) + " Cores"
    num_frameworks = data_df['Framework'].nunique()

    # Create dynamic legend key
    if core_mode_label == "combined":
        if num_frameworks > 1:
            data_df['Series_ID'] = data_df['Framework'] + \
                " (" + data_df['Cores_Label'] + ")"
        else:
            data_df['Series_ID'] = data_df['Cores_Label']
    else:
        data_df['Series_ID'] = data_df['Framework']

    # Custom styling
    CUSTOM_PALETTE = ["#1f77b4", "#d62728", "#2ca02c", "#9467bd", "#17becf"]
    CUSTOM_MARKERS = ["o", "D", "^", "s", "*", "P"]
    CUSTOM_DASHES = ["-", "--", ":", "-.", (5, 2, 1, 2)]

    series_list = data_df['Series_ID'].unique()

    # Plot each series explicitly with mean + error bars
    for idx, series_id in enumerate(series_list):
        series_df = data_df[data_df['Series_ID']
                            == series_id].sort_values('mem_mb')

        color = CUSTOM_PALETTE[idx % len(CUSTOM_PALETTE)]
        marker = CUSTOM_MARKERS[idx % len(CUSTOM_MARKERS)]
        linestyle = CUSTOM_DASHES[idx % len(CUSTOM_DASHES)]

        # Plot line + averaged markers
        ax.plot(
            series_df['mem_mb'],
            series_df['time_mean'],
            label=series_id,
            color=color,
            marker=marker,
            linestyle=linestyle,
            linewidth=2.5,
            markersize=8
        )

        # Draw trial error bars
        ax.errorbar(
            series_df['mem_mb'],
            series_df['time_mean'],
            yerr=series_df['time_err'],
            fmt='none',
            ecolor=color,
            capsize=4,
            capthick=1.5,
            elinewidth=1.5
        )

    # Labels and log scaling
    ax.set_ylabel("Seconds", fontsize=FONT_SIZE_AXIS_LABELS)

    ax.set_xscale('log', base=2)
    ax.get_xaxis().set_major_formatter(ScalarFormatter())

    unique_mems = sorted(data_df['mem_mb'].unique())
    ax.set_xticks(unique_mems)
    ax.set_xticklabels([str(m) for m in unique_mems])

    ax.set_xlabel("Memory Allocation Size (MB)",
                  fontsize=FONT_SIZE_AXIS_LABELS)
    ax.legend(title="Configuration", loc='best', fontsize=FONT_SIZE_LEGEND)

    # Save output
    output_filename = f"scaling_total_runtime_sec_t{
        TARGET_SERVER_THREADS}_{core_mode_label}.png"
    output_path = os.path.join(OUTPUT_DIR, output_filename)

    plt.savefig(output_path, dpi=DPI)
    plt.close(fig)
    print(f"Successfully generated plot: {output_path}")


def main():
    apply_publication_styles()
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    # Load and aggregate datasets for Total_Runtime_Sec
    dataset_dfs = [load_and_aggregate_runtime(ds) for ds in INPUT_DATASETS]
    valid_dfs = [d for d in dataset_dfs if d is not None]

    if not valid_dfs:
        print("Error: No valid datasets loaded.")
        return

    combined_df = pd.concat(valid_dfs, ignore_index=True)

    # 1. Combined Cores Plot (64 & 128 Cores together)
    combined_cores_df = combined_df[combined_df['proc_count'].isin(
        PROC_COUNTS)].copy()
    render_plot_variant(combined_cores_df, "combined")

    # 2. Individual Core Count Plots
    for proc in PROC_COUNTS:
        df_proc = combined_df[combined_df['proc_count'] == proc].copy()
        render_plot_variant(df_proc, f"{proc}cores")


if __name__ == "__main__":
    main()
