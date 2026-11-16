#!/usr/bin/env bash
# if script fails --> stop
set -euo pipefail

# move to root dir
# dirname "$0" takes the scripts folder
root_dir="$(cd "$(dirname "$0")"/.. && pwd)"

# run one 1 core, which is core 0 (p-core)
p_core_id=0

# check number of arguments
if (("$#" != 1)); then
    echo "Usage: bash run_branch.sh <label>" >&2
    exit 1
fi

# fetch data from arguments
label="$1"

# get the output file
mkdir -p "$root_dir/results"
output_file_name="$root_dir/results/branch_noise_$label.csv"
if [[ ! -f "$output_file_name" ]]; then
    touch "$output_file_name"
fi

# get the input file
# build cmake 
cmake -S "$root_dir" -B "$root_dir"/build
cmake --build "$root_dir"/build
input_file_name="$root_dir/build/branch"

# empty out output file
taskset -c "$p_core_id" "$input_file_name" --header > "$output_file_name"

# run the branch program 20 times.
reps=20
for (( i = 0; i < "$reps"; i ++ ))
do
    taskset -c "$p_core_id" "$input_file_name" "$label" 200 >> "$output_file_name"
done
