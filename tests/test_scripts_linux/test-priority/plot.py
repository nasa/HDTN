#!/usr/bin/env python3
"""Plot bundle priority over time"""
import argparse
import csv
from dataclasses import dataclass
import matplotlib.pyplot as plt
import numpy as np


@dataclass
class Stat:
    """Row in stats csv"""

    timestamp: int
    priority: int
    exp: int
    dest_node_id: int
    dest_srv_id: int
    src_node_id: int
    src_srv_id: int
    src_to_sink: int
    lifetime: int
    creation: int


def do_dot_plot(stats, ax):
    """Plot received bundles vs time"""
    priorities = [0, 1, 2]
    colors = ["green", "orange", "red"]
    for p in priorities:
        x_data = [p for stat in stats if stat.priority == p]
        y_data = [stat.timestamp for stat in stats if stat.priority == p]
        ax.plot(
            x_data,
            y_data,
            color=colors[p],
            linestyle="-",
            marker="o",
        )

        if y_data:
            delta = max(y_data) - min(y_data)
            print(f"Time to send for priority {p}: {delta:.2f}")
    ax.set_title("Priority arrival times")
    ax.legend(["priority 0 (bulk)", "priority 1 (normal)", "priority 2 (high)"])
    ax.set_xlabel("priority")
    ax.set_ylabel("relative arrival time (ms)")


def do_rate_plot(stats, ax):
    """Plot bundle rate over time"""
    times = np.array([stat.timestamp for stat in stats])
    deltas = []
    for i in range(len(times) - 1):
        deltas.append(times[i + 1] - times[i])
    width = 50
    avg_deltas = np.convolve(np.array(deltas), np.ones(width), "valid") / width
    x_vals = np.array(times[width - 1 : -1])
    y_vals = np.array(1000 / avg_deltas)
    ax.plot(x_vals, y_vals, "b-o")

    ax.set_ylim(ymin=0)

    ax.set_title("Bundle rate")
    # ax.legend(["priority 0 (bulk)", "priority 1 (normal)", "priority 2 (high)"])
    ax.set_xlabel("time (ms)")
    ax.set_ylabel("Bundles per second")


def print_stats(stats):
    """Print some summary stats"""
    src_to_sinks = np.array([stat.src_to_sink for stat in stats])
    print(f"Number of records: {len(stats)}")
    print(
        "src to sink: {}-{} ({:.2f}) seconds".format(
            src_to_sinks.min(), src_to_sinks.max(), src_to_sinks.mean()
        )
    )
    start_time = 1.0 * stats[0].timestamp / 1000.0
    end_time = 1.0 * stats[-1].timestamp / 1000.0
    delta_time = end_time - start_time / 1000.0
    rate = 1.0 * len(stats) / delta_time
    print(
        f"start: {start_time:.02f} "
        f"end: {end_time:.02f} "
        f"delta: {delta_time:.02f} "
        f"rate: {rate:.02f}"
    )

    num_one_bundles = sum(1 for stat in stats if stat.priority == 1)
    num_two_bundles = sum(1 for stat in stats if stat.priority == 2)

    print(
        f"Number of bundles: {len(stats)}, "
        f"by priority: normal: {num_one_bundles}, "
        f"expedited: {num_two_bundles}"
    )


def main():
    """Plot bundle priority over time"""
    parser = argparse.ArgumentParser(description="Plot priority over time")
    parser.add_argument("statfile", type=argparse.FileType("r"))
    args = parser.parse_args()

    reader = csv.reader(args.statfile)

    stats = []
    next(reader, None)  # Skip header
    for row in reader:
        stats.append(Stat(*(int(s) for s in row)))
    print_stats(stats)

    _, axs = plt.subplots(1, 2)

    do_dot_plot(stats, axs[0])

    do_rate_plot(stats, axs[1])

    plt.show()


if __name__ == "__main__":
    main()
