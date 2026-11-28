import sys
from pathlib import Path
import pandas as panda
import matplotlib.pyplot as plt

# check arguments
if len(sys.argv) < 2:
    print("Usage: python3 plot_probe_stability.py <input_file_name>", file=sys.stderr)
    sys.exit(1)

# get the file
input_file_path = Path(sys.argv[1])

# check the file to see if it exists
if not input_file_path.is_file():
    print("Error: Input file does not exist. Try again!", file=sys.stderr)
    sys.exit(1)
# check the file to see if it is a csv file
if input_file_path.suffix.lower() != ".csv":
    print("Error: Input file is not csv format. Try again!", file=sys.stderr)
    sys.exit(1)

# use pandas to read csv file
panda_dataframe = panda.read_csv(input_file_path)

fig, ax = plt.subplots(figsize=(9, 5))
# one row of statistics per buffer size, printed as a table after the loop
summary_rows = []

# one group per buffer size --> one line + one CV per size
for buffer_size, buffer_size_rows in panda_dataframe.groupby("buffer_size_bytes"):
    # each size has one row per call --> no need to group further
    ns_per_load = buffer_size_rows["ns_per_load"]
    size_mb = buffer_size / 1024**2

    # CV = std / mean: noise relative to the typical value (exit check: <= 2%)
    mean = ns_per_load.mean()
    cv_percent = ns_per_load.std() / mean * 100

    summary_rows.append({
        "size_mb": size_mb,
        "mean_ns": mean,
        "median_ns": ns_per_load.median(),
        "min_ns": ns_per_load.min(),
        "max_ns": ns_per_load.max(),
        "cv_percent": cv_percent,
    })

    ax.plot(buffer_size_rows["call"], ns_per_load, marker=".", linewidth=1,
            label=f"{size_mb:g} MB (CV {cv_percent:.1f}%)")

# print the table for the notebook
summary = panda.DataFrame(summary_rows)
print(summary.to_string(index=False, float_format=lambda value: f"{value:.2f}"))

ax.set_xlabel("call")
ax.set_ylabel("ns per load")
ax.set_title("Cache probe stability: 100 calls per buffer size")
# sizes range from L2 (~few ns) to L3 (~50 ns): log scale keeps every line visible
ax.set_yscale("log")
ax.legend()
fig.tight_layout()

# save the figure next to the csv
fig.savefig(input_file_path.with_suffix(".png"))
