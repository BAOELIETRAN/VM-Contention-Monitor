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
    stats = mode_rows.groupby("buffer_size_bytes")["ns_per_load"].agg(["median", "min", "max"])
    x = stats.index
    ax.plot(x, stats["median"], marker="o", label=mode)
    ax.fill_between(x, stats["min"], stats["max"], alpha=0.3)
ax.legend()
ax.set_xlabel("buffer size")
ax.set_ylabel("ns per load")
ax.set_xscale("log", base=2)
ax.set_yscale("log")
ax.set_xticks(x)
labels = ["4 KB", "8 KB", "16 KB", "32 KB", "64 KB", "128 KB",
          "256 KB", "512 KB", "1 MB", "2 MB", "4 MB", "8 MB",
          "16 MB", "32 MB", "64 MB", "128 MB", "256 MB", "512 MB"]
ax.set_xticklabels(labels, rotation=45)
# draw lines representing cache
caches = {"L1 (48 KB)": 48 * 1024,
          "L2 (2.5 MB)": 2.5 * 1024**2,
          "L3 (12 MB)": 12 * 1024**2}
for name, size in caches.items():
    ax.axvline(size, linestyle="--", color="gray", linewidth=1)
    ax.text(size, 0.95, name, transform=ax.get_xaxis_transform(),
            rotation=90, ha="right", va="top", fontsize=8, color="gray")
# save the figure
fig.savefig(input_file_path.with_suffix(".png"))