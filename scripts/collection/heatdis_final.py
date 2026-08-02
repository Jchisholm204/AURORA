#!/usr/bin/env python3
import os
import re
import pandas as pd

RESTORE_FP = re.compile(
    r".*_p(\d+)_i(\d+)_m(\d+)(?:_b(\d+))?_restore_test\.log$")
TEST_FP = re.compile(r".*_p(\d+)_i(\d+)_m(\d+)(?:_b(\d+))?_test\.log$")


def parse_aurora_logs(log_dir: str, file_pattern: re.Pattern) -> pd.DataFrame:
    """
    Parses a directory of benchmark logs by matching configurable suffix schemas.

    Supports:
        - ..._p${PROCS}_i${ITERATION}_m${MEM_MB}_test.log
        - ..._p${PROCS}_i${ITERATION}_m${MEM_MB}_b${BACKEND_PROCS}_test.log

    Args:
        log_dir (str): Path to the directory containing the log files.

    Returns:
        pd.DataFrame: Cleaned dataframe tracking configurations and runtimes.
    """

    # Regex to capture the total runtime inside the log file
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

        # Backend procs is optional; default to None (or 0) if it isn't part of the filename schema
        backend_match = match.group(4)
        backend_procs = int(
            backend_match) if backend_match is not None else None

        filepath = os.path.join(log_dir, filename)
        runtime = None

        try:
            with open(filepath, 'r', errors='ignore') as f:
                for line in f:
                    if runtime_match := runtime_pattern.search(line):
                        runtime = float(runtime_match.group(1))
                        break
        except IOError:
            continue

        if runtime is not None:
            data_rows.append({
                "Filename": filename,
                "MPI_Processes": mpi_processes,
                "Trial_Iteration": trial_idx,
                "Memory_MB": memory_mb,
                "Backend_Processes": backend_procs,
                "Total_Runtime_Sec": runtime
            })

    return pd.DataFrame(data_rows)


if __name__ == "__main__":
    # ==========================================
    # CONFIGURABLE VARIABLES
    # ==========================================
    VERSION = '0.0.3'
    TEST = 'heatdis_orig'
    INPUT_LOG_DIR = f"./results/{VERSION}/{TEST}/"
    # To collect final times
    OUTPUT_CSV_PATH = f"./results/{VERSION}/{TEST}_final_times.csv"
    PATTERN = TEST_FP
    # To collect restore times
    # OUTPUT_CSV_PATH = f"./results/{VERSION}/{TEST}_restore_times.csv"
    # PATTERN = RESTORE_FP
    # ==========================================

    print(f"Scanning target test logs in: {INPUT_LOG_DIR}")

    try:
        benchmark_df = parse_aurora_logs(INPUT_LOG_DIR, PATTERN)

        if not benchmark_df.empty:
            # Sort layout: Group logically by primary scale dimensions
            sort_columns = ["MPI_Processes", "Memory_MB"]
            if "Backend_Processes" in benchmark_df.columns and benchmark_df["Backend_Processes"].notna().any():
                sort_columns.append("Backend_Processes")
            sort_columns.append("Trial_Iteration")

            benchmark_df = benchmark_df.sort_values(
                by=sort_columns).reset_index(drop=True)

            benchmark_df.to_csv(OUTPUT_CSV_PATH, index=False)
            print(f"Successfully wrote {
                  len(benchmark_df)} test runs to '{OUTPUT_CSV_PATH}'")
            print("\nPreview of extracted data data:")
            print(benchmark_df.to_string(index=False, max_rows=15))
        else:
            print(
                "\n[!] No matching test logs containing valid execution strings found.")

    except Exception as e:
        print(f"Error executing script: {e}")
