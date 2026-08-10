## Block Test Heatmap Statistics
The `block_test.py` script can be used to generate statistics from [Collecting Results - Block Tests](Collecting%20Results.md#Block%20Tests).
By default, results will be saved to CSV files located under `results/$TESTS_VERSION/block_stats`.
Set at the top of the file, `MEM_FILTER_MB` and `TARGET_PROC_COUNT` limit the data displayed in the output.

```sh
python3 ./scripts/statistics/block_test.py
```

## Heat Distribution Statistics
The `heatdis_compare.py` script can be used to generate statistics from [Collecting Results - Summary Times](Collecting%20Results.md#Summary%20Times).
By default, results will be saved to CSV files located under `results/$TESTS_VERSION/heatdis_stats`.
Set at the top of the file, `MEM_FILTER_MB` and `TARGET_PROC_COUNT` limit the data displayed in the output.

```sh
python3 ./scripts/statistics/block_test.py
```

Note that the statistics mentioned within the rows of the output file measure the times reported by a single rank, and are not averaged across multiple trials.
To properly guage the difference between statistics, use the `Difference` colum and other column statistics.