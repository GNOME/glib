#!/usr/bin/env python3

"""
This script checks Meson configuration logs to verify no installed file is
missing installation tag.
"""

import argparse
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("builddir", type=Path)
    args = parser.parse_args()

    success = True
    path = args.builddir / "meson-info" / "intro-install_plan.json"
    with path.open(encoding="utf-8") as f:
        install_plan = json.load(f)
        for target in install_plan.values():
            for info in target.values():
                if not info["tag"]:
                    print("Missing install_tag for", info["destination"])
                    success = False
    return 0 if success else 1


if __name__ == "__main__":
    exit(main())
