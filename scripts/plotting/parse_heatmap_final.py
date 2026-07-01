#!/usr/bin/env python3
import os
import re
import pandas as pd

def parse_aurora_logs(log_dir: str) -> pd.DataFrame:
    """
    Parses a directory of benchmark logs by matching a strict suffix configuration.
    
    Extracts MPI processes and iteration counts specifically from files ending
    with '_p<ranks>_i<trial>_test.log', grabbing the execution runtime from within.
    
    Args:
        log_dir (str): Path to the directory containing the log files.
        
    Returns:
        pd.DataFrame: Cleaned dataframe tracking configurations and runtimes.
    """
    # Regex targets the exact suffix pattern regardless of what comes before it
    # Matches: anywhere_p128_i0_test.log -> p=128, i=0
    file_pattern = re.compile(r".*_p(\d+)_i(\d+)_test\.log$")
    
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
        filepath = os.path.join(log_dir, filename)
        
        runtime = None
        try:
            with open(filepath, 'r', errors='ignore') as f:
                for line in f:
                    if runtime_match := runtime_pattern.search(line):
                        runtime = float(runtime_match.group(1))
                        break
        except IOError:
            continue  # Skip unreadable files Safely
                    
        if runtime is not None:
            data_rows.append({
                "Filename": filename,
                "MPI_Processes": mpi_processes,
                "Trial_Iteration": trial_idx,
                "Total_Runtime_Sec": runtime
            })
            
    return pd.DataFrame(data_rows)


if __name__ == "__main__":
    # ==========================================
    # CONFIGURABLE VARIABLES
    # ==========================================
    INPUT_LOG_DIR = "./results/0.0.1-2/heatdis_veloc/"
    OUTPUT_CSV_PATH = "veloc_benchmark_summary.csv"
    # ==========================================

    print(f"Scanning target test logs in: {INPUT_LOG_DIR}")
    
    try:
        benchmark_df = parse_aurora_logs(INPUT_LOG_DIR)
        
        if not benchmark_df.empty:
            # Sort explicitly by processes, then iteration
            benchmark_df = benchmark_df.sort_values(
                by=["MPI_Processes", "Trial_Iteration"]
            ).reset_index(drop=True)
            
            benchmark_df.to_csv(OUTPUT_CSV_PATH, index=False)
            print(f"Successfully wrote {len(benchmark_df)} test runs to '{OUTPUT_CSV_PATH}'")
            print("\nPreview of extracted data:")
            print(benchmark_df.to_string(index=False, max_rows=10))
        else:
            print("\n[!] No matching '_test.log' runs with valid runtimes were found.")
            
    except Exception as e:
        print(f"Error executing script: {e}")
