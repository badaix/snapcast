#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# This file is part of snapcast
# Copyright (C) 2022-2024  Johannes Pohl
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

import sys
import re

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: changelog_current_version.py <changelog.md file>")
        sys.exit(1)

    data = ""
    start_found = False
    with open(sys.argv[1], 'r') as file:
        for line in file:
            if line.startswith("_"):
                break
            if not start_found:
                start_found = line.startswith("## Version ")
                if not start_found:
                    continue
            if line.startswith("##"):
                line = line[1:]
            data += line

    print(data)
