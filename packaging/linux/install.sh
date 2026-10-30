#!/bin/sh
# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
# Adapted from RF StateSide BrickSuite's Python-backed archive installer.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec python3 "$root/installer.py" install "$@"
