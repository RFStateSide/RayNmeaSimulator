# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
"""Reproduce the offline diagnostic catalog; never run/download during app builds.

Input is CANboat's Apache-2.0 generated database at the pinned revision below.
Use --source for an existing verified copy, or explicitly --fetch to retrieve it.
Updating the revision/hash is an intentional developer review, not runtime work.
"""
import argparse
import hashlib
import json
from pathlib import Path
import urllib.request

REVISION = "e3b0e18a3eaa6f24242b49adfd9043767a5dd373"
SHA256 = "a7cfc2ec5cc282c2ac31cea5ce427afdffc0cdf3cf21a2b73a4738fd1584a283"
URL = f"https://raw.githubusercontent.com/canboat/canboat/{REVISION}/docs/canboat.json"
TARGETS = {65359, 126208, 126720, 129044, 129542, 129547, 131056,
           65379, 130916, 126993, 130820, 65535, 65311}
LOOKUP_LIMITS = {"MANUFACTURER_CODE": 2047, "DEVICE_CLASS": 127, "INDUSTRY_CODE": 7,
                 "SEATALK_PILOT_MODE_16": 65535, "SEATALK_PILOT_MODE": 255,
                 "SEATALK_DEVICE_ID": 255, "FUSION_POWER_STATE": 255,
                 "FUSION_MUTE_COMMAND": 255}


def generate(data):
    if hashlib.sha256(data).hexdigest() != SHA256:
        raise ValueError("CANboat source hash differs from the reviewed reference")
    source = json.loads(data)
    lookups = {}
    for group in source["LookupEnumerations"]:
        if group["Name"] in LOOKUP_LIMITS:
            entries = group["EnumValues"]
            values = [e["Value"] for e in entries]
            if len(values) != len(set(values)):
                raise ValueError("Duplicate lookup code")
            maximum = LOOKUP_LIMITS[group["Name"]]
            if any(not isinstance(v, int) or not 0 <= v <= maximum for v in values):
                raise ValueError("Lookup code outside its reviewed field width")
            lookups[group["Name"]] = {str(e["Value"]): e["Name"] for e in entries}
    functions = next(g for g in source["LookupIndirectEnumerations"] if g["Name"] == "DEVICE_FUNCTION")
    pairs = [(e["Value1"], e["Value2"]) for e in functions["EnumValues"]]
    if len(pairs) != len(set(pairs)) or any(not 0 <= cls <= 127 or not 0 <= fn <= 255 for cls, fn in pairs):
        raise ValueError("Duplicate or invalid class/function lookup")
    lookups["DEVICE_FUNCTION"] = {f'{e["Value1"]}:{e["Value2"]}': e["Name"] for e in functions["EnumValues"]}
    names, variants = {}, []
    for row in source["PGNs"]:
        pgn = row["PGN"]
        if not row.get("Fallback") and str(pgn) not in names:
            names[str(pgn)] = row["Description"]
        if pgn not in TARGETS or row.get("Fallback"):
            continue
        matches = [dict(offset=f["BitOffset"], bits=f["BitLength"], value=f["Match"])
                   for f in row["Fields"] if "Match" in f and f.get("BitOffset") is not None]
        variants.append(dict(pgn=pgn, id=row["Id"], name=row["Description"],
                             length=row.get("Length", row.get("MinLength", 0)), matches=matches))
    return dict(provenance=dict(url=URL, revision=REVISION, sha256=SHA256,
                               version=source["Version"], license=source["License"]),
                lookups=lookups, names=names, variants=variants)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--source", type=Path)
    group.add_argument("--fetch", action="store_true")
    parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parents[1] / "resources/nmea2000/lookups.json")
    args = parser.parse_args()
    data = args.source.read_bytes() if args.source else urllib.request.urlopen(URL, timeout=30).read()
    result = generate(data)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    # Canonical UTF-8/LF bytes, independent of Windows newline translation.
    args.output.write_bytes((json.dumps(result, sort_keys=True, ensure_ascii=False, indent=2) + "\n").encode("utf-8"))


if __name__ == "__main__":
    main()
