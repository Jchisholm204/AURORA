# Test Run Workflow
1. Source the `env.sh` script in `scripts` to set up:
    - The Base Repo Directory Variables
    - Cluster specific script exports
2. Source the `launch.sh` script in `scripts/launch` to assist with using the `launch.batch` script
    - `launch.batch` is the actual batch script.
    - `launch.batch` is a generic script built for cross-platform, cli/srv launches
    - `launch.sh` provides a structured way to launch `launch.batch`
3. Run one of the `scripts/tests` scripts to
    - Compile the code for that test
    - launch several slurm jobs to run the test multiple times
