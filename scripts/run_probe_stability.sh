#!/usr/bin/env bash

set -euo pipefail

# check the number of arguments
if (( "$#" != 0 )); then
    echo "Usage: bash run_probe_stability.sh" >&2
    exit 1
fi

# move to root dir
root_dir="$(cd "$(dirname "$0")"/.. && pwd)"

# run on core 0
p_core_id=0

gov_file=/sys/devices/system/cpu/cpu${p_core_id}/cpufreq/scaling_governor
turbo_file=/sys/devices/system/cpu/intel_pstate/no_turbo

# get the input file
cmake -S "$root_dir" -B "$root_dir"/build
cmake --build "$root_dir"/build
input_file_name="$root_dir"/build/probe_stability

# get the output files
# csv: the data; log: calibration lines (TSC GHz, loads per call) for the notebook
mkdir -p "$root_dir"/results
output_file_name="$root_dir"/results/probe_stability.csv
log_file_name="$root_dir"/results/probe_stability.log

# ask for password
sudo -v

# save the original settings, and restore them whenever the script exits
orig_gov=$(cat "$gov_file")
orig_turbo=$(cat "$turbo_file")

restore() {
    # || true: if one restore fails, still try the other (set -e would stop here)
    echo "$orig_gov"      | sudo tee "$gov_file"   > /dev/null || true
    echo "$orig_turbo"    | sudo tee "$turbo_file" > /dev/null || true
    echo "restored: governor=$orig_gov, no_turbo=$orig_turbo" >&2
}
trap restore EXIT

if [[ "$orig_gov" != "performance" ]]; then
    echo performance | sudo tee "$gov_file" > /dev/null
fi

if [[ "$orig_turbo" == "0" ]]; then
    echo 1 | sudo tee "$turbo_file" > /dev/null
fi

# check the settings really changed (a failed write would silently add noise)
if [[ "$(cat "$gov_file")" != "performance" || "$(cat "$turbo_file")" != "1" ]]; then
    echo "Error: could not set governor=performance and no_turbo=1" >&2
    exit 1
fi

# write the header once (> empties the files first)
"$input_file_name" --header > "$output_file_name"
: > "$log_file_name"

# run one probe: stdout (csv rows) -> csv file,
# stderr (calibration info) -> shown on screen AND saved to the log file
run_probe() {
    local label="$1"
    shift
    echo "== $label ==" | tee -a "$log_file_name" >&2
    taskset -c "$p_core_id" "$input_file_name" "$@" \
        >> "$output_file_name" \
        2> >(tee -a "$log_file_name" >&2)
}

# explicit sizes (6 MB left out: the default run below is 6 MB on this machine,
# and two runs with the same size would merge into one group in the plot)
size_mb_list=(1 4 8 16)

for size_mb in "${size_mb_list[@]}"; do
    buffer_size_bytes=$(( size_mb * 1024 * 1024 ))
    run_probe "size=${size_mb} MB" "$buffer_size_bytes"
done

# default size: no argument -> probe_stability uses rec_buffer_size() (half of L3)
run_probe "size=default (rec_buffer_size)"
