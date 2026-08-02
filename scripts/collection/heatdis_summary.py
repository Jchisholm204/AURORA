#!/usr/bin/env python3
import os
import re
import pandas as pd

# ==========================================
# CONFIGURABLE VARIABLES
# ==========================================
VERSION = "0.0.3"
TEST = "heatdis_aurora"
INPUT_LOG_DIR = f"./results/{VERSION}/{TEST}/"
OUTPUT_CSV_PATH = f"./results/{VERSION}/{TEST}_summary_times.csv"
# ==========================================


def parse_logs(log_dir: str) -> pd.DataFrame:
    """
    Parses a directory of benchmark logs by matching log filenames and regular expressions.

    Returns:
        pd.DataFrame: Long-form raw dataframe containing metric events.
    """
    file_pattern = re.compile(
        r".*_p(\d+)_i(\d+)_m(\d+)(?:_b(\d+))?_test\.log$"
    )

    timer_app_pattern = re.compile(
        r"\[TIMER_APP\]\s+(?P<metric>.+?)\s+\(rank\s+(?P<rank>\d+)\)\s+:\s+(?P<ms>[\d.]+)\s+ms"
    )
    timer_lib_pattern = re.compile(
        r"\[TIMER\]\s+(?P<metric>.+?)\s+:\s+(?P<ms>[\d.]+)\s+ms"
    )
    runtime_pattern = re.compile(r"Execution finished in ([\d.]+) seconds\.")

    data_rows = []

    if not os.path.isdir(log_dir):
        raise FileNotFoundError(f"The directory '{log_dir}' does not exist.")

    for filename in os.listdir(log_dir):
        match = file_pattern.match(filename)
        if not match:
            continue

        mpi_processes = int(match.group(1))
        trial_idx = int(match.group(2))
        memory_mb = int(match.group(3))

        backend_match = match.group(4)
        backend_procs = int(
            backend_match) if backend_match is not None else None

        filepath = os.path.join(log_dir, filename)

        try:
            with open(filepath, "r", errors="ignore") as f:
                for line in f:
                    line = line.strip()

                    # 1. Capture inner granular TIMER_APP metrics
                    if app_match := timer_app_pattern.search(line):
                        data = app_match.groupdict()
                        data_rows.append({
                            "MPI_Processes": mpi_processes,
                            "Memory_MB": memory_mb,
                            "Backend_Processes": backend_procs,
                            "Trial_Iteration": trial_idx,
                            "Metric": data["metric"].strip(),
                            "Time": float(data["ms"]),
                        })

                    # 2. Capture underlying library [TIMER] metrics
                    elif lib_match := timer_lib_pattern.search(line):
                        data = lib_match.groupdict()
                        data_rows.append({
                            "MPI_Processes": mpi_processes,
                            "Memory_MB": memory_mb,
                            "Backend_Processes": backend_procs,
                            "Trial_Iteration": trial_idx,
                            "Metric": f"lib_{data['metric'].strip()}",
                            "Time": float(data["ms"]),
                        })

                    # 3. Capture master end-to-end job runtime execution string
                    elif runtime_match := runtime_pattern.search(line):
                        data_rows.append({
                            "MPI_Processes": mpi_processes,
                            "Memory_MB": memory_mb,
                            "Backend_Processes": backend_procs,
                            "Trial_Iteration": trial_idx,
                            "Metric": "Total_Execution_Sec",
                            "Time": float(runtime_match.group(1)),
                        })

        except IOError:
            print(f"Warning: Could not read file {filename}. Skipping.")
            continue

    return pd.DataFrame(data_rows)


def format_data(raw_df: pd.DataFrame) -> pd.DataFrame:
    """
    Aggregates raw log metrics by calculating min, mean, median, and max figures
    across all ranks for each metric within a trial iteration.

    Returns:
        pd.DataFrame: A wide-format dataframe where each row represents a single trial.
    """
    if raw_df.empty:
        return pd.DataFrame()

    group_cols = ["MPI_Processes", "Memory_MB",
                  "Backend_Processes", "Trial_Iteration"]

    # 1. Group by trial parameters + metric to compute aggregate stats across ranks.
    # dropna=False is CRITICAL so NaN values in Backend_Processes are not dropped.
    stats_df = (
        raw_df.groupby(group_cols + ["Metric"], dropna=False)["Time"]
        .agg(["min", "mean", "median", "max"])
        .reset_index()
    )

    # 2. Pivot the aggregated metrics wide (creating columns like Metric_min, Metric_mean, etc.)
    pivoted = stats_df.pivot(
        index=group_cols,
        columns="Metric",
        values=["min", "mean", "median", "max"],
    )

    # 3. Flatten MultiIndex columns into 'Metric_stat' naming format (e.g., Checkpoint_mean, Checkpoint_median)
    pivoted.columns = [f"{metric}_{stat}" for stat, metric in pivoted.columns]
    formatted_df = pivoted.reset_index()

    # Sort rows cleanly by test configuration parameters and trial iteration
    sort_cols = [col for col in group_cols if col in formatted_df.columns]
    formatted_df = formatted_df.sort_values(
        by=sort_cols).reset_index(drop=True)

    return formatted_df


def output_csv(df: pd.DataFrame, output_path: str) -> None:
    """
    Saves the formatted dataframe to a CSV file and prints a preview to stdout.
    """
    if df.empty:
        print("[!] No benchmark data found to process.")
        return

    # Ensure output destination directory exists
    output_dir = os.path.dirname(output_path)
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)

    df.to_csv(output_path, index=False)
    print(f"Successfully generated summary report ({
          len(df)} trials) -> '{output_path}'\n")
    # print("Preview of Formatted Trial Summary Table:")
    # print(df.to_string(index=False, max_rows=10))


if __name__ == "__main__":
    print(f"Scanning target test logs in: {INPUT_LOG_DIR}")
    try:
        raw_data = parse_logs(INPUT_LOG_DIR)
        if raw_data.empty:
            print("No RAW Data")
        formatted_data = format_data(raw_data)
        output_csv(formatted_data, OUTPUT_CSV_PATH)
    except Exception as e:
        print(f"Error executing script: {e}")
