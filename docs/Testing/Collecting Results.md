- All collection scripts should be run from the root directory of the project.
- The `VERSION` variable must be set to the correct test version.
- The `TEST` name variable must be set to the name of test to collect data from, matching the names set in `run_tests.sh` and the results folder.
- Always ensure all paths are correct before attempting to run a collection script.
## Block Tests
The blocking benchmark measures the synthetic performance of AURORA by  completing a single checkpoint/restore cycle on artifitial data.
This test is useful to both confirm base performance, but also to verify the integrity of restored data.

Logs from the block test are saved to
`results/version/block_test_aurora_heatmap` by default.

The results can be extracted from the logs using the block test collection script, located under `scripts/collection/block_test.py`.
The test version, as well as input and output data directories can be specified at the top of the file.
When run, the file will output a CSV containing the raw data extracted from block test.
This data may be futher processed into heatmaps using the plotting script, described in [[Generating Plots#Block Test Heatmaps]], or the statistics script, described in [[Generating Statistics#Block Test Heatmap Statistics]].

## Heat Distribution Benchmark
The heat distribution benchmark is used to measure the applicable performance of AURORA, when compared to VELOC.

### ALL Times
The `heatdis_all.py` script, located under `scripts/collection`, can be used to extract all raw data from the heat distribution benchmark.
The test version, name, as well as the input and output data directories can be specified at the bottom of the file.
### Summary Times
The Heat Distribution Summary times script (`heatdis_summary.py`) extracts and processes log data.
The result is a cleanly formatted CSV containing the minimum, mean, median, and maximum times for each sample recorded during a test run.
Note that statistics in the CSV are calculated across all ranks in a single run, thus the minimum reported time is the minimum reported time from a single rank within the run.
The test version, and other parameters can be specified at the top of the file located in `scripts/collection`.
The data produced by this script may be plotted using the scripts described in [[Generating Plots#Heat Distribution Benchmark]].

### Final/Restart Times
To collect only the final execution times or the restart times, use the `heatdis_final.py` collection script.
The result of this script will be a cleanly formatted CSV containing only the final execution times as reported by the Heat Distribution benchmark.
Configuration variables are located near the bottom of the file.
As with the other collection scripts, the collected results depend on the selected `TEST` name parameter.

*Note:* The `PATTERN` variable must be correctly set for results to be collected.
Setting the `PATTERN` variable to `TEST_FP` (File Pattern) will return the final execution times recorded from either the:
1. AURORA checkpoint test
2. VELOC checkpoint test
3. Original Benchmark (no checkpoint restore mechanism)
The data produced by this script may be plotted using the scripts described in [[Generating Plots#Checkpoint Benchmarks]]
Futher statistics can also be calculated using the scripts described in [[Generating Statistics#Heat Distribution Statistics]].

Alternatively, setting the `PATTERN` variable to `RESTORE_FP` will return the final execution times recorded from the:
1. AURORA restart benchmark (restarts from final iteration and finishes)
2. VELOC restart benchmark (restarts from final iteration and finishes)
The data produced by this script may be plotted using the scripts described in [[Generating Plots#Restart Time]].


