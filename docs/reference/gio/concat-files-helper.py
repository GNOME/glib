#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# Copyright (C) 2018 Collabora Inc.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General
# Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
#
# Author: Xavier Claessens <xavier.claessens@collabora.com>

import sys

if len(sys.argv) < 3:
    print(
        "Usage: {} <output file> <input file 1> ...".format(
            os.path.basename(sys.argv[0])
        )
    )
    sys.exit(1)

with open(sys.argv[1], "w") as outfile:
    for fname in sys.argv[2:]:
        with open(fname) as infile:
            for line in infile:
                outfile.write(line)
