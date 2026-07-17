import os
import re
import json
import pandas as pd

# ==========================================
# CONFIGURATION: Set your path here
# ==========================================

VERSION = '0.0.2-3'
TEST = 'block_test_aurora'
DATA_ROOT_FOLDER = f"./results/{VERSION}/{TEST}/"
OUTPUT_FILE = f"./results/{VERSION}/{TEST}_times.csv"
# ==========================================


def parse_aurora_logs(root_dir):
    all_data = []

    # Regex for filename: processor count, iteration, memory, backend procs
    file_regex = re.compile(r'p(\d+)_i(\d+)_m(\d+)_b(\d+)_test')
    # file_regex = re.compile(r'p(\d+)_i(\d+)_m(\d+)_test')

    print(f"Scanning directory: {root_dir}")

    for root, dirs, files in os.walk(root_dir):
        # Determine server threads from the directory name
        for filename in files:
            if not filename.endswith(".log"):
                continue

            # Extract metadata from filename
            meta = file_regex.search(filename)
            if not meta:
                continue

            mem_mb = int(meta.group(3))
            proc_count = int(meta.group(1))
            run_id = int(meta.group(2))
            server_threads = int(meta.group(4))
            # server_threads = -1

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
                                "mem_mb": mem_mb,
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
                                "mem_mb": mem_mb,
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
