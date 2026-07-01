import re
import os
import glob
import sys
import pandas as pd

# ==========================================
# CONFIGURATION
# ==========================================
# Path to the directory containing your benchmark folders
RESULTS_DIR = "./results/0.0.1-2/heatdis_aurora16/"
OUTPUT_CSV_FILE = "aurora_sweep_results.csv"
# ==========================================

# Regex to parse variables from the log filename
# Captures: benchmark_name, process_count (p), iteration (i)
# Example: heatdis_aurora16_p128_i3_test.log -> ('heatdis_aurora16', '128', '3')
PATTERN_FILENAME = re.compile(
    r"(?P<bench>[\w-]+)_p(?P<p>\d+)_i(?P<i>\d+)_test\.log"
)

# Regex to parse the contents inside the files
PATTERN_APP = re.compile(
    r"\[TIMER_APP\]\s+(?P<name>.+?)\s+\(rank\s+(?P<rank>\d+)\)\s+:\s+(?P<ms>[\d.]+)\s+ms"
)
PATTERN_EXEC = re.compile(
    r"Execution finished in\s+(?P<total_sec>[\d.]+)\s+seconds"
)

def parse_single_log(file_path, bench_name, p_count, iter_id):
    """Parses an individual _test.log file."""
    records = []
    execution_time = None
    occurrence_counters = {}

    with open(file_path, 'r') as f:
        for line in f:
            line = line.strip()
            
            # 1. Match Timer App
            match_app = PATTERN_APP.search(line)
            if match_app:
                data = match_app.groupdict()
                rank = int(data["rank"])
                timer_name = data["name"].strip()
                ms = float(data["ms"])
                
                # Track occurrence position per rank and timer name
                counter_key = (rank, timer_name)
                occurrence = occurrence_counters.get(counter_key, 0)
                occurrence_counters[counter_key] = occurrence + 1
                
                records.append({
                    "benchmark": bench_name,
                    "p_processes": p_count,
                    "test_iteration": iter_id,
                    "rank": rank,
                    "occurrence": occurrence,
                    "timer_name": timer_name,
                    "ms": ms
                })
                continue
            
            # 2. Match Execution End String
            match_exec = PATTERN_EXEC.search(line)
            if match_exec:
                execution_time = float(match_exec.group("total_sec"))

    if not records:
        return pd.DataFrame()

    # Convert to DataFrame
    df_long = pd.DataFrame(records)

    # Pivot this single file's metrics into columns
    df_pivoted = df_long.pivot(
        index=["benchmark", "p_processes", "test_iteration", "rank", "occurrence"],
        columns="timer_name",
        values="ms"
    ).reset_index()

    # Append overall test runtime metric if found
    if execution_time is not None:
        df_pivoted["test_execution_seconds"] = execution_time
    else:
        df_pivoted["test_execution_seconds"] = None

    return df_pivoted


def process_all_logs(directory_path):
    """Finds and parses all matching _test.log files in the target directory."""
    # Build search path for any _test.log file in the directory
    search_path = os.path.join(directory_path, "*_test.log")
    target_files = glob.glob(search_path)
    
    if not target_files:
        print(f"No '_test.log' files found in path: {directory_path}", file=sys.stderr)
        return pd.DataFrame()

    all_dfs = []
    print(f"Found {len(target_files)} log files to parse...")

    for file_path in target_files:
        filename = os.path.basename(file_path)
        match_meta = PATTERN_FILENAME.match(filename)
        
        if match_meta:
            meta = match_meta.groupdict()
            bench_name = meta["bench"]
            p_count = int(meta["p"])
            iter_id = int(meta["i"])
            
            # Parse the individual log file
            df_file = parse_single_log(file_path, bench_name, p_count, iter_id)
            if not df_file.empty:
                all_dfs.append(df_file)
        else:
            print(f"Skipping file (name format doesn't match expected _p#_i# pattern): {filename}")

    if not all_dfs:
        return pd.DataFrame()

    # Combine everything into a single master sheet
    master_df = pd.concat(all_dfs, ignore_index=True)
    
    # Sort for readability: group by benchmark, process size, iteration run, then ranks
    sort_cols = ["benchmark", "p_processes", "test_iteration", "rank", "occurrence"]
    master_df = master_df.sort_values(by=sort_cols).reset_index(drop=True)
    
    return master_df


if __name__ == "__main__":
    print(f"Starting crawl of: {RESULTS_DIR}")
    master_df = process_all_logs(RESULTS_DIR)
    
    if not master_df.empty:
        master_df.to_csv(OUTPUT_CSV_FILE, index=False)
        print(f"\nSuccess! Combined sweep data written to: {OUTPUT_CSV_FILE}")
        print("\nMaster Data Preview:")
        print(master_df.head(10))
    else:
        print("No valid benchmark data could be compiled.")
