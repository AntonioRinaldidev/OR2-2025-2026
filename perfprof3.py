#!/usr/bin/env python3 (vers. 20-apr-2026)

from __future__ import annotations

import argparse
import csv
from itertools import cycle
from pathlib import Path
from typing import TYPE_CHECKING, Any, Optional

if TYPE_CHECKING:
    import numpy as np
    from numpy.typing import NDArray


# Parameters
BIG_M = 1e6
INFINITY = float("inf")

# Constants for plotting
LINE_WIDTH = 1.2  # Default line width
MARK_SIZE = 7  # Default marker size
MAX_STYLES = 10  # Maximum number of unique styles (markers/colors)
DASHES = cycle(("-", "--", "-.", ":"))
MARKERS = cycle(("o", "s", "^", "D", "v", "p", "*", "x", "P", "H"))
COLORS = cycle(("#1f77b4", "#ff7f0e", "#2ca02c", "#d62728", "#9467bd", "#8c564b", "#e377c2", "#7f7f7f", "#bcbd22", "#17becf"))


class CmdLineParser:
    def __init__(self) -> None:
        self.parser = argparse.ArgumentParser(
            description=("Performance profile plotting tool.\nRequires numpy and matplotlib (pip install -r requirements.txt)"),
            epilog='Example usage: python3 %(prog)s -T 3600 -S 2 -M 20 lagr.csv pp.pdf -P "all instances, shift 2 sec.s"',
            formatter_class=lambda prog: argparse.RawDescriptionHelpFormatter(prog, max_help_position=50),
        )

        # Positional arguments
        self.parser.add_argument("input", type=Path, help="Input CSV file")
        self.parser.add_argument("output", type=Path, help="Output PDF file")

        # Optional arguments
        self.parser.add_argument("-B", "--bw", dest="bw", action="store_true", help="plot in B/W (default: %(default)s)")
        self.parser.add_argument("-D", "--delimiter", dest="delimiter", default=",", help="delimiter for input files (default: %(default)s)")
        self.parser.add_argument("-L", "--logplot", dest="logplot", action="store_true", help="plot in log scale for x (default: %(default)s)")
        self.parser.add_argument("-M", "--maxratio", dest="maxratio", default=2.0, type=float, help="maxratio for perf. profile (default: %(default)s)")
        self.parser.add_argument("-N", "--no-max-cols", dest="nomaxcols", action="store_true", help=f"do not limit the number of columns to {MAX_STYLES} (markers on plots will repeat) (default: %(default)s)")
        self.parser.add_argument("-P", "--plot-title", dest="plottitle", default=None, help="plot title (default: %(default)s)")
        self.parser.add_argument("-S", "--shift", dest="shift", default=0.0, type=float, help="shift for data (default: %(default)s)")
        self.parser.add_argument("-T", "--timelimit", dest="timelimit", default=float("inf"), type=float, help="time limit for runs in seconds (default: %(default)s)")
        self.parser.add_argument("-X", "--x-label", dest="xlabel", default="Time Ratio", help="x axis label (default: %(default)s)")

        self.namespace: Optional[argparse.Namespace] = None

    def parseArgs(self) -> argparse.Namespace:
        """Parses arguments and returns the namespace object."""
        args = self.parser.parse_args()

        # Validate args

        args.input = args.input.resolve()
        if not args.input.exists():
            self.parser.error(f"Input file not found: '{args.input}'.")
        if not args.input.is_file():
            self.parser.error(f"Input path is not a file: '{args.input}'.")

        args.output = args.output.resolve()
        if not args.output.parent.exists():
            self.parser.error(f"Output directory does not exist: '{args.output.parent}'.")
        if args.output.exists() and not args.output.is_file():
            self.parser.error(f"Output path is a directory, not a file: '{args.output}'.")

        if args.maxratio <= 0 and args.maxratio != -1:
            self.parser.error(f"Invalid value for maxratio: {args.maxratio}. Must be > 0, or -1 for auto-scaling.")
        if args.shift < 0:
            self.parser.error(f"Invalid value for shift: {args.shift}. Must be >= 0.")
        if args.timelimit <= 0:
            self.parser.error(f"Invalid value for timelimit: {args.timelimit}. Must be > 0.")

        self.namespace = args
        return self.namespace

    def printOptions(self) -> None:
        """Utility method to print options in a formatted way."""
        if not self.namespace:
            return

        def sort_args(key: str, val: Any) -> tuple[str, str]:
            """Sorting key function for options."""
            if isinstance(val, Path):
                sort_type = "aaa"
            elif key == "delimiter":
                sort_type = "aab"
            elif val is None:
                sort_type = "str"
            else:
                sort_type = type(val).__name__
            return (sort_type, key)

        items = tuple((k, v) for k, v in vars(self.namespace).items())

        MAX_LABEL_LEN = max(len(label) for label, _ in items) if items else 0
        MAX_VAL_LEN = max(len(str(value)) for _, value in items) if items else 0
        TABLE_WIDTH = MAX_LABEL_LEN + MAX_VAL_LEN + 4

        # Print in a formatted table
        title = " RUNTIME CONFIGURATION "
        print(f"\n{title:=^{TABLE_WIDTH}}")
        for label, value in sorted(items, key=lambda item: sort_args(*item)):
            # Convert Paths or Nones to clean strings
            print(f"{label.upper():<{MAX_LABEL_LEN}}    {value}")
        print("=" * TABLE_WIDTH + "\n")


def readTable(opt: argparse.Namespace) -> tuple[list[str], list[str], NDArray[np.float64]]:
    """
    Read a CSV file with performance profile specification. The format is as follows:

    ```
    ncols algo1 algo2 ...
    instance_name time(algo1) time(algo2) ...
    ...
    ```
    """
    import numpy as np

    with open(opt.input, "r", newline="") as fp:
        reader = csv.reader(fp, delimiter=opt.delimiter)
        firstline = next(reader)

        ALGO_COUNT = len(firstline) - 1
        NCOLS = int(firstline[0])
        if NCOLS > ALGO_COUNT:
            print(f"[WARNING] Specified number of columns ({NCOLS}) exceeds the number of algorithms provided ({ALGO_COUNT}). Reducing to match the number of algorithms.")
            NCOLS = ALGO_COUNT
        if NCOLS < ALGO_COUNT:
            print(f"[WARNING] Specified number of columns ({NCOLS}) is less than the number of algorithms provided ({ALGO_COUNT}). Only the first {NCOLS} algorithms will be considered.")

        if not opt.nomaxcols and NCOLS > MAX_STYLES:
            raise ValueError(f"Number of columns ({NCOLS}) exceeds the maximum allowed ({MAX_STYLES}). Use -N or --no-max-cols to proceed with marker repetition.")

        cnames: list[str] = [name.strip() for name in firstline[1:]]
        rnames: list[str] = []
        rows: list[list[float]] = []

        for line_num, row in enumerate(reader, start=2):
            # Skip blank lines
            if not row:
                continue

            instance_name = row[0].strip()

            # Enforce matrix structure
            if len(row) < NCOLS + 1:
                raise ValueError(f"Line {line_num} (Instance '{instance_name}') is malformed: Expected {NCOLS + 1} columns, found {len(row)}.")

            rnames.append(instance_name)
            rows.append([float(x) for x in row[1 : NCOLS + 1]])

    data: NDArray[np.float64] = np.array(rows)

    return rnames, cnames, data


def main():
    parser = CmdLineParser()
    opt = parser.parseArgs()
    parser.printOptions()

    import matplotlib
    import matplotlib.pyplot as plt
    import numpy as np

    matplotlib.use("PDF")

    # Read data
    instance_names, algo_names, data = readTable(opt)
    shape: tuple[int, int] = data.shape
    nrows, ncols = shape

    # Add shift
    data: NDArray[np.float64] = data + opt.shift
    # Compute ratios
    minima: NDArray[np.float64] = data.min(axis=1)[:, np.newaxis]
    ratio: NDArray[np.float64] = data / minima
    # Compute maxratio
    if opt.maxratio == -1:
        opt.maxratio = ratio.max()
    # Any time >= timelimit will count as maxratio + bigM (so that it does not show up in plots)
    ratio[data >= opt.timelimit] = opt.maxratio + BIG_M
    # Sort ratios
    ratio.sort(axis=0)

    # Plotting
    y: NDArray[np.float64] = np.arange(nrows, dtype=np.float64) / nrows
    for j in range(ncols):
        options: dict[str, Any] = dict(
            label=algo_names[j],
            linewidth=LINE_WIDTH,
            linestyle=next(DASHES),
            drawstyle="steps-post",
            marker=next(MARKERS),
            markeredgewidth=LINE_WIDTH,
            markersize=MARK_SIZE,
            color=next(COLORS),
        )

        # Apply B/W settings if specified
        if opt.bw:
            options["markerfacecolor"] = "w"
            options["markeredgecolor"] = "k"
            options["color"] = "k"

        # Plot with log scale if specified
        if opt.logplot:
            plt.semilogx(ratio[:, j], y, **options)
        else:
            plt.plot(ratio[:, j], y, **options)

    # Formatting
    plt.xlim(1, opt.maxratio)
    plt.ylim(0, 1)
    plt.xlabel(opt.xlabel)
    plt.legend(loc="lower right")
    if opt.plottitle is not None:
        plt.title(opt.plottitle)

    # Save the figure in PDF format
    plt.savefig(opt.output.with_suffix(".pdf"))


if __name__ == "__main__":
    main()
