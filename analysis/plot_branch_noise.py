import sys
from pathlib import Path
import pandas as panda
import matplotlib.pyplot as plt

# check arguments
if len(sys.argv) < 2:
    print("Usage: python3 plot_branch_noise.py <input_file_name>")
    sys.exit(1)

# get the file
input_file_path = Path(sys.argv[1])

# check the file to see if it exists
if input_file_path.is_file() == False:
    print("Error: Input file does not exist. Try again!")
    sys.exit(1)
# check the file to see if it is a csv file (check file extension)
if input_file_path.suffix.lower() != ".csv":
    print("Error: Input file is not csv format. Try again!")
    sys.exit(1)

# use pandas to read csv file
# load csv file into the panda dataframe
panda_dataframe = panda.read_csv(input_file_path)

# range = a --> (b - 1)
x = range(1, len(panda_dataframe) + 1)
# fig = the whole image (used to save it); ax = the chart we draw on and label
fig, ax = plt.subplots()
ax.plot(x, panda_dataframe["ns_per_elem"], marker="o")
ax.set_xlabel("run")
ax.set_ylabel("ns per element")
ax.set_title("rand branch")
ax.set_xticks(x)
# include a dashed median line
ax.axhline(panda_dataframe["ns_per_elem"].median(), linestyle="--")
ax.set_ylim(bottom=0)
ax.set_ylim(0, 2.5)
# save the figure
fig.savefig(input_file_path.with_suffix(".png"))