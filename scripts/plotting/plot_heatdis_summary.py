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
    {"path": f"{RESULTS_DIR}/heatdis_aurora_summary_times.csv", "label": "AURORA"},
    {"path": f"{RESULTS_DIR}/heatdis_veloc_summary_times.csv", "label": "VELOC"},
    {"path": f"{RESULTS_DIR}/heatdis_orig_summary_times.csv", "label": "Baseline"},
]

# 2. TARGET THREAD FILTERING:
TARGET_SERVER_THREADS = 16    # Target backend server threads (e.g., 16)

# 3. LIST OF METRIC KEYS TO BATCH PROCESS:
TARGET_METRIC_KEYS = [
    # "lib_Checkpoint Total",
    # "lib_Memory Copy",
    # "lib_Wait for checkpoint",
    # "lib_Mem DeReg Wait",
    "Checkpoint",
    "Total_Execution_Sec"
]

# 4. CORE COUNTS TO PLOT:
PROC_COUNTS = [64, 128]

# 5. ERROR BAR CONFIGURATION:
ERRORBAR_TYPE = 'sd'

TIME_UNIT = "s"  # Options: "ms" or "s"

# 6. PUBLICATION SIZING & STYLING:
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


def load_and_normalize_dataset(dataset_info, metric_key):
    """Loads a single CSV, standardizes schema differences, and filters by thread count."""
    filepath = dataset_info["path"]
    label = dataset_info["label"]

    if not os.path.exists(filepath):
        return None

    try:
        df = pd.read_csv(filepath)
    except Exception as e:
        print(f"Error reading {filepath}: {e}")
        return None

    # Map variant column names to unified names
    rename_dict = {}
    if 'MPI_Processes' in df.columns:
        rename_dict['MPI_Processes'] = 'proc_count'
    if 'Memory_MB' in df.columns:
        rename_dict['Memory_MB'] = 'mem_mb'
    if 'Backend_Processes' in df.columns:
        rename_dict['Backend_Processes'] = 'server_threads'

    df = df.rename(columns=rename_dict)

    # Locate time / latency metric column across different CSV structures
    target_val_col = None
    if 'metric' in df.columns:  # Long-format CSVs
        df = df[df['metric'] == metric_key].copy()
        for col_candidate in ['time_ms', 'time', 'latency']:
            if col_candidate in df.columns:
                target_val_col = col_candidate
                break
    else:  # Wide summary CSVs
        candidates = [
            f"{metric_key}_mean",
            metric_key,
            f"lib_{metric_key}_mean",
            f"lib_{metric_key}"
        ]
        for cand in candidates:
            if cand in df.columns:
                target_val_col = cand
                break

    if target_val_col is None or df.empty:
        return None

    # 1. Store base value
    raw_val = df[target_val_col]

    # 2. Check if metric is already native seconds (e.g. Total_Execution_Sec)
    is_already_seconds = "Total_Execution_Sec" in metric_key or "_sec" in metric_key.lower()

    # 3. Apply unit scaling
    if TIME_UNIT == "s":
        df['time_val'] = raw_val if is_already_seconds else raw_val / 1000.0
    else:  # TIME_UNIT == "ms"
        df['time_val'] = raw_val * 1000.0 if is_already_seconds else raw_val

    df['Framework'] = label

    # Apply thread filtering if server_threads exists in dataset
    if 'server_threads' in df.columns and TARGET_SERVER_THREADS is not None:
        unique_threads = df['server_threads'].dropna().unique()
        if len(unique_threads) > 1 and TARGET_SERVER_THREADS in unique_threads:
            df = df[df['server_threads'] == TARGET_SERVER_THREADS]

    return df


def render_plot_variant(data_df, metric_key, core_mode_label):
    """Generates and exports an individual plot variant with distinct black/white styles."""
    if data_df.empty:
        return

    sns.set_theme(style="whitegrid")
    fig, ax = plt.subplots(figsize=FIG_SIZE, layout="constrained")

    # Re-apply publication font parameters after setting seaborn theme
    apply_publication_styles()

    data_df['Cores_Label'] = data_df['proc_count'].astype(str) + " Cores"
    num_frameworks = data_df['Framework'].nunique()

    # Create a composite grouping key so Seaborn maps unique markers & dash styles to EVERY line
    if core_mode_label == "combined":
        if num_frameworks > 1:
            data_df['Series_ID'] = data_df['Framework'] + \
                " (" + data_df['Cores_Label'] + ")"
        else:
            data_df['Series_ID'] = data_df['Cores_Label']
    else:
        data_df['Series_ID'] = data_df['Framework']

    hue_col = "Series_ID"
    style_col = "Series_ID"

    # High-contrast publication color palette
    CUSTOM_PALETTE = ["#1f77b4", "#d62728", "#2ca02c", "#9467bd", "#17becf"]

    # Distinct markers for print legibility (Circle, Diamond, Triangle Up, Square, Star, Plus)
    CUSTOM_MARKERS = ["o", "D", "^", "s", "*", "P"]

    # Dash patterns for Seaborn (Solid, Dashed, Dotted, Dash-dot, Long dash)
    CUSTOM_DASHES = ["", (4, 2), (1, 1), (3, 1, 1, 1), (5, 2, 1, 2)]

    num_series = data_df['Series_ID'].nunique()

    sns.lineplot(
        data=data_df,
        x="mem_mb",
        y="time_val",
        hue=hue_col,
        style=style_col,
        markers=CUSTOM_MARKERS[:num_series],
        dashes=CUSTOM_DASHES[:num_series],
        linewidth=2.5,
        markersize=9,
        palette=CUSTOM_PALETTE[:num_series],
        errorbar=ERRORBAR_TYPE,
        err_style='bars',
        err_kws={'capsize': 4, 'capthick': 1.5},
        ax=ax
    )

    # Dynamic Y-axis Label based on selected TIME_UNIT
    unit_label = "Seconds" if TIME_UNIT == "s" else "Milliseconds"
    ax.set_ylabel(f"{unit_label}", fontsize=FONT_SIZE_AXIS_LABELS)

    # Log base-2 scaling for Memory MB
    ax.set_xscale('log', base=2)
    ax.get_xaxis().set_major_formatter(ScalarFormatter())

    # Align explicit ticks to actual memory sizes in dataset
    unique_mems = sorted(data_df['mem_mb'].unique())
    ax.set_xticks(unique_mems)
    ax.set_xticklabels([str(m) for m in unique_mems])

    # Labels
    ax.set_xlabel("Memory Allocation Size (MB)",
                  fontsize=FONT_SIZE_AXIS_LABELS)
    ax.legend(title="Configuration", loc='best', fontsize=FONT_SIZE_LEGEND)

    # Save PNG file
    metric_slug = metric_key.lower().replace(" ", "_").replace("/", "_")
    output_filename = f"scaling_{metric_slug}_t{
        TARGET_SERVER_THREADS}_{core_mode_label}.png"
    output_path = os.path.join(OUTPUT_DIR, output_filename)

    plt.savefig(output_path, dpi=DPI)
    plt.close(fig)
    print(f"Successfully generated: {output_path}")


def main():
    apply_publication_styles()
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    for metric_key in TARGET_METRIC_KEYS:
        # Load and combine all specified CSV datasets for the current metric
        dataset_dfs = [load_and_normalize_dataset(
            ds, metric_key) for ds in INPUT_DATASETS]
        valid_dfs = [d for d in dataset_dfs if d is not None]

        if not valid_dfs:
            print(f"Warning: No valid data found for metric '{
                  metric_key}'. Skipping.")
            continue

        combined_df = pd.concat(valid_dfs, ignore_index=True)

        # 1. Plot Combined View (Both 64 Cores and 128 Cores)
        combined_cores_df = combined_df[combined_df['proc_count'].isin(
            PROC_COUNTS)].copy()
        render_plot_variant(combined_cores_df, metric_key, "combined")

        # 2. Plot 64 Cores Only
        df_64 = combined_df[combined_df['proc_count'] == 64].copy()
        render_plot_variant(df_64, metric_key, "64cores")

        # 3. Plot 128 Cores Only
        df_128 = combined_df[combined_df['proc_count'] == 128].copy()
        render_plot_variant(df_128, metric_key, "128cores")


if __name__ == "__main__":
    main()
