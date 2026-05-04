import os
import re
import json
import pandas as pd

# ==========================================
# CONFIGURATION: Set your path here
# ==========================================
DATA_ROOT_FOLDER = "./tests_workers_32"
OUTPUT_FILE = "aurora32_bench_results.csv"
# ==========================================


def parse_aurora_logs(root_dir):
    all_data = []

    # Regex for filename: extracts memory (kb), process count (p), and run number
    # Example: run_block_test_mem16384kb_p32_run1.log
    file_regex = re.compile(r'mem(\d+)kb_p(\d+)_run(\d+)')

    # Regex for server threads from folder name (assumes folder contains "threads_X")
    thread_regex = re.compile(r'tests_workers_(\d+)')

    print(f"Scanning directory: {root_dir}")

    for root, dirs, files in os.walk(root_dir):
        # Determine server threads from the directory name
        thread_match = thread_regex.search(root)
        server_threads = int(thread_match.group(
            1)) if thread_match else "unknown"

        for filename in files:
            if not filename.endswith(".log"):
                continue

            # Extract metadata from filename
            meta = file_regex.search(filename)
            if not meta:
                continue

            mem_kb = int(meta.group(1))
            proc_count = int(meta.group(2))
            run_id = int(meta.group(3))

            file_path = os.path.join(root, filename)

            with open(file_path, 'r') as f:
                for line in f:
                    # 1. Parse JSON Rank Benchmarks
                    if "__RANK_BENCH__" in line:
                        try:
                            clean_json = line.split(
                                "__RANK_BENCH__")[1].strip()
                            data = json.loads(clean_json)
                            all_data.append({
                                "server_threads": server_threads,
                                "mem_kb": mem_kb,
                                "proc_count": proc_count,
                                "run_id": run_id,
                                "rank": data.get("rank"),
                                "metric": data.get("id"),
                                "time_ms": data.get("time")
                            })
                        except Exception as e:
                            continue

                    # 2. Parse Info Wait Timers
                    elif "[TIMER] Wait" in line:
                        wait_match = re.search(r':\s+([\d\.]+)\s+ms', line)
                        if wait_match:
                            all_data.append({
                                "server_threads": server_threads,
                                "mem_kb": mem_kb,
                                "proc_count": proc_count,
                                "run_id": run_id,
                                "rank": None,  # Wait timers aren't always tied to a rank ID line
                                "metric": "wait_timer",
                                "time_ms": float(wait_match.group(1))
                            })

    return pd.DataFrame(all_data)


if __name__ == "__main__":
    df = parse_aurora_logs(DATA_ROOT_FOLDER)

    if not df.empty:
        df.to_csv(OUTPUT_FILE, index=False)
        print(f"Successfully parsed {len(df)} entries into {OUTPUT_FILE}")
    else:
        print("No data found. Check your folder path and naming conventions.")
