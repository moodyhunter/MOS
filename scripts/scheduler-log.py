#!/usr/bin/env python3

import pandas as pd
import sys

if len(sys.argv) != 2:
    print("Usage: scheduler-log.py <log file>")
    sys.exit(1)

logfile = sys.argv[1]

# event keywords to look for in the log
KEYWORDS = ['switching to', 'leaving']


with open(logfile, 'r') as f:
    log_lines = f.read()

log_lines = log_lines.splitlines()

# Initialize a dictionary to hold the current thread for each CPU
cpu_state: dict[int, str | None] = {}
log_data = []

for lineid, line in enumerate(log_lines):
    if not any(keyword in line for keyword in KEYWORDS):
        continue

    line_split = line.split('|')[:4]
    line_split = [x.strip() for x in line_split]

    cpu_num = int(line_split[1].split()[1])

    if KEYWORDS[0] in line:
        thread = line.split(KEYWORDS[0] + ' ')[-1].split(',')[0]

        # detect if a thread is running on multiple CPUs
        if thread in cpu_state.values():
            print(f"Thread {thread} is running on multiple CPUs:")
            print()

            # print relevant log lines
            for i in range(lineid - 5, lineid + 5):
                if i < 0 or i >= len(log_lines):
                    continue
                if i == lineid:
                    print("\033[31m", log_lines[i], "\033[m", sep="")
                else:
                    print(log_lines[i], "\033[m", sep="")

    elif KEYWORDS[1] in line:
        thread = None

    cpu_state[cpu_num] = thread
    log_data.append(cpu_state.copy())

# Convert the log data to a DataFrame for easier viewing
df = pd.DataFrame(log_data)
df.columns = [f"CPU {i}" for i in df.columns]  # we can do this because dict keys are sorted
df['Notes'] = df.apply(lambda row: 'Multiple CPUs' if row.dropna().duplicated().any() else '', axis=1)

# sort the columns
df = df[[f"CPU {i}" for i in range(df.shape[1] - 1)] + ['Notes']]

# Display the table
print()
print(df.to_string())
