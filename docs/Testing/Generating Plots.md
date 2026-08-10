- Plotting scripts are located under `scripts/plotting`.
- All plotting scripts should be run from the root directory of the project.
- The `TESTS_VERSION` variable must be set to the correct test version.
- The `TEST` name variable must be set to the name of test to collect data from, matching the names set in `run_tests.sh` and the results folder.
- Always ensure all paths are correct before attempting to run a collection script.
## Block Test Heatmaps
The `plot_heatmap.py` script can be used to plot sorted heatmaps of target metrics captured by the [Block Test](Collecting%20Results.md#Block%20Tests).
When run, the file will output seperate heatmaps for each of the metrics specified in `TARGET_METRICS`.
Before plotting, values are sorted smallest to largest to produce a gradient scale rather than a scattered plot.
All heatmaps will be stored in the `OUTPUT_DIR` directory.

```sh
python3 ./scripts/plotting/plot_heatmap.py
```

## Heat Distribution Benchmark

### Checkpoint Benchmarks
The `plot_heatdis_summary.py` script can be used to generate plots from the data recorded by [Collecting Results - Summary Times](Collecting%20Results.md#Summary%20Times).
When run, the script will generate seperate and combined plots of the 64 and 128 core benchmarks.
Each metric specified by `TARGET_METRIC_KEYS` will be placed in its own plot.
The datasets being plotted can be changed by adding or removing the input files from the `INPUT_DATASETS` list.
Plot settings, including the Y axis time unit, can be changed by setting the `TIME_UNIT` variable.
All plots will output to the `OUTPUT_DIR` directory.

```sh
python3 ./scripts/plotting/plot_heatdis_summary.py
```

### Restart Time
The `plot_heatdis_restore.py` script plots data captured by [Collecting Results - Final/Restart Times](Collecting%20Results.md#Final/Restart%20Times).
`TESTS_VERSION`,  `INPUT_DATASETS`, and the output directory can be specified at the top of the file.
By defult, they output to `results/$TESTS_VERSION/figures`.
When run, the script will generate seperate and combined plots of the 64 and 128 core benchmarks.

```sh
python3 ./scripts/plotting/plot_heatdis_restore.py
```
