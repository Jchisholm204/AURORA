import os
import pandas as pd

# ==========================================
# CONFIGURATION
# ==========================================
TESTS_VERSION = "0.0.3"
RESULTS_DIR = f"./results/{TESTS_VERSION}"
OUTPUT_DIR = f"{RESULTS_DIR}/heatdis_stats"

# 1. MULTI-CSV INPUT CONFIGURATION
INPUT_DATASETS = [
    {
        "path": f"{RESULTS_DIR}/heatdis_aurora_summary_times.csv",
        "label": "aurora",
    },
    {
        "path": f"{RESULTS_DIR}/heatdis_veloc_summary_times.csv",
        "label": "veloc",
    },
    # {
    #     "path": f"{RESULTS_DIR}/heatdis_orig_summary_times.csv",
    #     "label": "baseline",
    # },
]

# 2. COMPARISON PAIR DEFINITION (Matching labels in INPUT_DATASETS)
TEST_A = "aurora"
TEST_B = "veloc"

# 3. STATISTICAL VARIABLES TO PROCESS IN SPECIFIED ORDER
VARS = ["Mean", "StdDev", "Min", "Q1", "Median", "Q3", "Max"]

# 4. DATA FILTERING
TARGET_PROC_COUNT = 64  # Options in CSVs: 64, 128
MEM_FILTER_MB = 64  # Options in CSVs: 64, 128, 256, 512, 1024
TARGET_SERVER_THREADS = [
    0,
    # 8,
    16,
]  # Restrict thread counts (or None for all)

# 5. LIST OF METRIC KEYS TO BATCH PROCESS:
TARGET_METRIC_KEYS = [
    # "lib_Checkpoint Total",
    # "lib_Memory Copy",
    # "lib_Wait for checkpoint",
    # "lib_Mem DeReg Wait",
    "Checkpoint",
    "Total_Execution_Sec",
]

# 6. STATISTICAL SUFFIXES TO MATCH IN CSVs
STAT_SUFFIXES = ["_mean", "_median", "_min", "_max"]

# 7. OUTPUT OPTIONS
EXPORT_COMBINED_CSV = True
EXPORT_PER_METRIC_CSV = False
# ==========================================


def load_and_transform_datasets(datasets, metric_keys, suffixes):
    """Loads CSVs, standardizes metadata column names, matches requested metric

    keys, and converts to a unified long-format DataFrame.
    """
    loaded_dfs = []

    for item in datasets:
        path = item["path"]
        label = item["label"]

        if not os.path.exists(path):
            print(f"Warning: File not found '{path}'. Skipping '{label}'.")
            continue

        df = pd.read_csv(path)

        # Standardize metadata columns
        df = df.rename(
            columns={
                "MPI_Processes": "proc_count",
                "Memory_MB": "mem_mb",
                "Backend_Processes": "server_threads",
            }
        )

        df["server_threads"] = df["server_threads"].fillna(0).astype(int)
        df["dataset"] = label

        meta_cols = [
            "proc_count",
            "mem_mb",
            "server_threads",
            "Trial_Iteration",
            "dataset",
        ]

        # Match columns for requested metric keys
        target_cols = []
        for key in metric_keys:
            if key in df.columns:
                target_cols.append(key)
            for suff in suffixes:
                col_name = f"{key}{suff}"
                if col_name in df.columns:
                    target_cols.append(col_name)

        target_cols = list(dict.fromkeys(target_cols))
        if not target_cols:
            continue

        melted = df.melt(
            id_vars=meta_cols,
            value_vars=target_cols,
            var_name="metric",
            value_name="time_sec",
        )

        loaded_dfs.append(melted)

    if not loaded_dfs:
        return None

    return pd.concat(loaded_dfs, ignore_index=True)


def generate_statistical_summary():
    master_df = load_and_transform_datasets(
        INPUT_DATASETS, TARGET_METRIC_KEYS, STAT_SUFFIXES
    )

    if master_df is None or master_df.empty:
        print("Error: No matching datasets or metrics found. Exiting.")
        return

    os.makedirs(OUTPUT_DIR, exist_ok=True)
    all_summaries = []

    discovered_metrics = master_df["metric"].unique()

    for target_metric in discovered_metrics:
        # Filter rows by config selections
        mask = (
            (master_df["metric"] == target_metric)
            & (master_df["mem_mb"] == MEM_FILTER_MB)
            & (master_df["proc_count"] == TARGET_PROC_COUNT)
        )

        if TARGET_SERVER_THREADS is not None:
            mask &= master_df["server_threads"].isin(TARGET_SERVER_THREADS)

        plot_df = master_df[mask].copy()

        if plot_df.empty:
            print("\n" + "=" * 80)
            print(
                f"Warning: No rows matched filter choice for metric '{
                    target_metric}'."
            )
            print("=" * 80)
            continue

        print("\n" + "=" * 80)
        print(f"STATISTICAL SUMMARY & COMPARISON FOR METRIC: {target_metric}")
        print(
            f"Configuration: {MEM_FILTER_MB} MB Checkpoint | {
                TARGET_PROC_COUNT} MPI Procs"
        )
        print(f"Comparison Pair: TEST_A='{TEST_A}' vs TEST_B='{TEST_B}'")
        print("=" * 80)

        # Calculate base statistics for TEST_A and TEST_B across trial iterations
        stats_by_dataset = {}
        for dataset_name, group in plot_df.groupby("dataset"):
            s = group["time_sec"]
            stats_by_dataset[dataset_name] = {
                "Mean": s.mean(),
                "StdDev": s.std(),
                "Min": s.min(),
                "Q1": s.quantile(0.25),
                "Median": s.median(),
                "Q3": s.quantile(0.75),
                "Max": s.max(),
            }

        # Build output row following the exact requested loop pattern
        row_data = {"metric": target_metric}

        for V in VARS:
            val_a = stats_by_dataset.get(TEST_A, {}).get(V, float("nan"))
            val_b = stats_by_dataset.get(TEST_B, {}).get(V, float("nan"))

            diff = val_a - val_b
            pct_diff = (
                ((val_a - val_b) / val_b) * 100 if val_b != 0 else float("nan")
            )

            row_data[f"{V} - {TEST_A}"] = val_a
            row_data[f"{V} - {TEST_B}"] = val_b
            row_data[f"{V} Difference"] = diff
            row_data[f"{V} Percentage Difference"] = pct_diff

        summary_df = pd.DataFrame([row_data])

        # Display structured output columns
        pd.set_option("display.max_columns", None)
        pd.set_option("display.width", 1000)

        print("\nFormatted Column Outputs:")
        for col in summary_df.columns:
            if col != "metric":
                val = summary_df[col].values[0]
                unit = (
                    "%" if "Percentage" in col else "s"
                )  # Format percentage vs time
                sign = "+" if ("Difference" in col and val > 0) else ""
                print(f"  {col:<35} : {sign}{val:,.3f} {unit}")

        if EXPORT_PER_METRIC_CSV:
            out_path = f"{
                OUTPUT_DIR}/summary_{target_metric}_{MEM_FILTER_MB}mb_{TARGET_PROC_COUNT}p.csv"
            summary_df.to_csv(out_path, index=False)
            print(f"\nSaved metric summary to: {out_path}")

        all_summaries.append(summary_df)

    if EXPORT_COMBINED_CSV and all_summaries:
        combined_df = pd.concat(all_summaries, ignore_index=True)
        combined_out_path = f"{
            OUTPUT_DIR}/summary_all_metrics_{MEM_FILTER_MB}mb_{TARGET_PROC_COUNT}p.csv"
        combined_df.to_csv(combined_out_path, index=False)
        print("\n" + "=" * 80)
        print(f"Master summary CSV saved to: {combined_out_path}")
        print("=" * 80)


if __name__ == "__main__":
    generate_statistical_summary()
