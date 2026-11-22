#!/usr/bin/env bash

set -euo pipefail

# check number of arguments
if (( "$#" != 0 )); then
    echo "Usage: bash run_chase.sh" >&2
    exit 1
fi

# move to root dir
root_dir="$(cd "$(dirname "$0")"/.. && pwd)"

# run on core 0
p_core_id=0

gov_file=/sys/devices/system/cpu/cpu${p_core_id}/cpufreq/scaling_governor
turbo_file=/sys/devices/system/cpu/intel_pstate/no_turbo

# get the input file (build at full speed, before changing settings)
cmake -S "$root_dir" -B "$root_dir"/build
cmake --build "$root_dir"/build
input_file_name="$root_dir"/build/chase

# get the output file
mkdir -p "$root_dir"/results
output_file_name="$root_dir"/results/cache_chase_all_mode.csv
if [[ ! -f "$output_file_name" ]]; then
    touch "$output_file_name"
fi

# ask for password when we need it
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

# add header to output file
echo "run,$("$input_file_name" --header)" > "$output_file_name"

# run through all the run modes, and run each mode 5 times
reps=5
run_mode_list=("random" "sequential" "stride")
for run_mode in "${run_mode_list[@]}"; do
    echo "Current run mode: ${run_mode}"
    for (( i = 1; i <= "$reps"; i ++ ))
    do
        taskset -c "$p_core_id" "$input_file_name" "$run_mode" | sed "s/^/$i,/" >> "$output_file_name"
    done
done
