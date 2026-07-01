#!/usr/bin/env python3
import os
import re
import pandas as pd

def parse_aurora_logs(log_dir: str) -> pd.DataFrame:
    """
    Parses a directory of benchmark logs by matching configurable suffix schemas.
    
    Extracts individual TIMER_APP internal metrics, underlying library TIMER metrics,
    and final total runtime, organizing them into a tidy, long-form dataframe layout.
    
    Args:
        log_dir (str): Path to the directory containing the log files.
        
    Returns:
        pd.DataFrame: Cleaned long-form dataframe structured for analytics.
    """
    # Filename schema pattern
    file_pattern = re.compile(r".*_p(\d+)_i(\d+)_m(\d+)(?:_b(\d+))?_test\.log$")
    
    # Content extraction patterns
    timer_app_pattern = re.compile(
        r"\[TIMER_APP\]\s+(?P<metric>.+?)\s+\(rank\s+(?P<rank>\d+)\)\s+:\s+(?P<ms>[\d.]+)\s+ms"
    )
    # Pattern to extract library timers, ignoring any leading timestamp/logging boilerplate
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
        backend_procs = int(backend_match) if backend_match is not None else None
        
        filepath = os.path.join(log_dir, filename)
        
        try:
            with open(filepath, 'r', errors='ignore') as f:
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
                            "Rank": int(data["rank"]),
                            "Metric": data["metric"].strip(),
                            "Time": float(data["ms"])
                        })
                    
                    # 2. Capture underlying library [TIMER] metrics
                    elif lib_match := timer_lib_pattern.search(line):
                        data = lib_match.groupdict()
                        
                        data_rows.append({
                            "MPI_Processes": mpi_processes,
                            "Memory_MB": memory_mb,
                            "Backend_Processes": backend_procs,
                            "Trial_Iteration": trial_idx,
                            "Rank": -1, # Explicitly set to -1 for clarity as a global/unknown rank metric
                            "Metric": f"lib_{data['metric'].strip()}",
                            "Time": float(data["ms"])
                        })
                    
                    # 3. Capture master end-to-end job runtime execution string
                    elif runtime_match := runtime_pattern.search(line):
                        data_rows.append({
                            "MPI_Processes": mpi_processes,
                            "Memory_MB": memory_mb,
                            "Backend_Processes": backend_procs,
                            "Trial_Iteration": trial_idx,
                            "Rank": -1, # Global boundary identifier flag
                            "Metric": "Total_Execution_Sec",
                            "Time": float(runtime_match.group(1))
                        })
                        
        except IOError:
            print(f"Warning: Could not read file {filename}. Skipping.")
            continue
            
    return pd.DataFrame(data_rows)


if __name__ == "__main__":
    # ==========================================
    # CONFIGURABLE VARIABLES
    # ==========================================
    VERSION = '0.0.1-2'
    TEST = 'heatdis_aurora'
    INPUT_LOG_DIR = f"./results/{VERSION}/{TEST}/"
    OUTPUT_CSV_PATH = f"./results/{VERSION}/{TEST}_all_times.csv"
    # ==========================================

    print(f"Scanning target test logs in: {INPUT_LOG_DIR}")
    
    try:
        benchmark_df = parse_aurora_logs(INPUT_LOG_DIR)
        
        if not benchmark_df.empty:
            # Layout Sorting Hierarchy: Sort by parameters, iterations, rankings, then keep metric streams linear
            sort_columns = ["MPI_Processes", "Memory_MB"]
            if "Backend_Processes" in benchmark_df.columns and benchmark_df["Backend_Processes"].notna().any():
                sort_columns.append("Backend_Processes")
            
            sort_columns.extend(["Trial_Iteration", "Rank", "Metric"])
            
            benchmark_df = benchmark_df.sort_values(by=sort_columns).reset_index(drop=True)
            
            # Save final dataset
            benchmark_df.to_csv(OUTPUT_CSV_PATH, index=False)
            print(f"Successfully wrote {len(benchmark_df)} individual metrics to '{OUTPUT_CSV_PATH}'")
            print("\nPreview of Tidy Data Rows (Rank -1 captures global & library data):")
            print(benchmark_df.to_string(index=False, max_rows=15))
        else:
            print("\n[!] No matching test logs containing valid execution or timing strings found.")
            
    except Exception as e:
        print(f"Error executing script: {e}")
