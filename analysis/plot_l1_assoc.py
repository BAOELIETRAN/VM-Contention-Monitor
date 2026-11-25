import sys
from pathlib import Path
import pandas as panda
import matplotlib.pyplot as plt

# check arguments
if len(sys.argv) < 2:
    print("Usage: python3 plot_chase.py <input_file_name>")
    sys.exit(1)

# get the file
input_file_path = Path(sys.argv[1])

# check the file to see if it exists
if input_file_path.is_file() == False:
    print("Error: Input file does not exist. Try again!")
    sys.exit(1)
# check the file to see if it is a csv file
if input_file_path.suffix.lower() != ".csv":
    print("Error: Input file is not csv format. Try again!")
    sys.exit(1)    

# use pandas to read csv file
panda_dataframe = panda.read_csv(input_file_path)
fig, ax = plt.subplots()
for mode, mode_rows in panda_dataframe.groupby("run_mode"):
    stats = mode_rows.groupby("num_cache_lines")["ns_per_load"].agg(["median", "min", "max"])
    x = stats.index
    ax.plot(x, stats["median"], marker="o", label=mode)
    ax.fill_between(x, stats["min"], stats["max"], alpha=0.3)
ax.legend()
ax.set_xlabel("num cache lines")
ax.set_ylabel("ns per load")
# ticks: every K that appears in the data (all modes), sorted
k_values = sorted(panda_dataframe["num_cache_lines"].unique())
ax.set_xticks(k_values)

# draw a line at the L1 associativity boundary:
# K <= 12 fits in one set's 12 ways, K = 13 is the first that doesn't
# --> put the line halfway between 12 and 13
l1_ways = 12
l1_label = f"L1 = {l1_ways} ways"
boundary = l1_ways + 0.5
ax.axvline(boundary, linestyle="--", color="gray", linewidth=1)
# x in data units (K), y as a fraction of the axis height (0.95 = near the top)
ax.text(boundary, 0.95, l1_label, transform=ax.get_xaxis_transform(),
        rotation=90, ha="right", va="top", fontsize=8, color="gray")
# save the figure
fig.savefig(input_file_path.with_suffix(".png"))