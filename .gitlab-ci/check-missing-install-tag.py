#!/usr/bin/env python3

"""
This script checks Meson configuration logs to verify no installed file is
missing installation tag.
"""

import argparse
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("builddir", type=Path)
    args = parser.parse_args()

    logfile = args.builddir / "meson-logs" / "meson-log.txt"
    with logfile.open(encoding="utf-8") as f:
        if "Failed to guess install tag" in f.read():
            print(
                f"Some files are missing install_tag, see {logfile} for details."  # no-qa
            )
            return 1
    return 0


if __name__ == "__main__":
    exit(main())
