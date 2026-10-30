# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
"""Read-only source notice and provenance-registration checks; no dependency acquisition."""
import csv
import json
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[1]
with (root / "docs/provenance-inventory.csv").open(encoding="utf-8", newline="") as stream:
    rows = {row["path"]: row for row in csv.DictReader(stream)}
errors = []
checked = 0
exceptions = []
internal_reuse = {
    "src/aboutdialog.cpp", "src/aboutlinks.h", "src/credentialstore.cpp",
    "src/applicationlog.cpp", "src/applicationlog.h", "src/logviewer.cpp",
    "src/logviewer.h",
}
for folder in ("src", "tests"):
    for path in sorted((root / folder).rglob("*")):
        if path.suffix not in (".cpp", ".h"):
            continue
        name = path.relative_to(root).as_posix()
        row = rows.get(name)
        if row is None:
            errors.append(name + ": missing provenance registration")
            continue
        if name in internal_reuse:
            if row["category"] != "A" or row["license"] != "AGPL-3.0-or-later":
                errors.append(name + ": internal reuse incorrectly classified")
            if "Owner attestation 2026-10-28" not in row["evidence"]:
                errors.append(name + ": missing owner-attested origin evidence")
        if row["category"] == "C":
            exceptions.append(name)
            continue
        text = path.read_text(encoding="utf-8")
        if name in internal_reuse and "Adapted from RF StateSide BrickSuite" not in text:
            errors.append(name + ": missing historical adaptation credit")
        if not text.startswith("/*\n * RayNmeaSimulator - NMEA2000/NMEA0183 Marine Simulator"):
            errors.append(name + ": missing approved project header")
        if text.count("This file is part of RayNmeaSimulator.") != 1:
            errors.append(name + ": duplicate/missing project notice")
        if "GNU Affero General" not in text[:1600] or "RF StateSide LLC" not in text[:1600]:
            errors.append(name + ": incomplete project notice")
        checked += 1
for name, row in rows.items():
    path = root / name
    if not path.is_file():
        if row["disposition"] != "removed from current tree":
            errors.append(name + ": registered file missing")
        continue
    try:
        if path.suffix == ".json":
            json.loads(path.read_text(encoding="utf-8"))
        elif path.suffix in (".ui", ".qrc"):
            ET.parse(path)
    except (ValueError, ET.ParseError) as error:
        errors.append(name + ": invalid structured format: " + str(error))
print(json.dumps({"project_cpp_headers": checked, "reference_exceptions": exceptions,
                  "registered_files": len(rows), "errors": errors}, indent=2))
sys.exit(bool(errors))
