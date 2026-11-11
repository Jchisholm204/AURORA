#!/usr/bin/env zsh

# Blocking Read/Write AURORA Test - DPU-offloaded restore
#
# Builds the regular block test with AUL_DEFAULT_RESTART_REMOTE defined so
# that AUL_Restart() restores on the DPU rather than the host. No test code
# is duplicated; the standard block test script performs the run.

export AUL_DEFAULT_RESTART_REMOTE=ON
exec "${0:A:h}/block_test_aurora.zsh" "$@"
