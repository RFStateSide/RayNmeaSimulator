# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
"""Hash native settings and historical layouts; never print/export their values."""
import argparse
import hashlib
import json
import os
from pathlib import Path


def snapshot():
    result = {'files': {}, 'registry': {}}
    home = Path.home()
    roots = [home/'.raynmeasimulator']
    if os.name != 'nt':
        roots.append(Path(os.environ.get('XDG_CONFIG_HOME', home/'.config'))/'RFStateSide')
    for index, root in enumerate(roots):
        result['files'][str(index)] = {'exists': root.exists(), 'sha256': {
            str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(root.rglob('*')) if p.is_file()}}
    if os.name == 'nt':
        import winreg
        def walk(key):
            values, children = [], {}
            nkeys, nvalues, _ = winreg.QueryInfoKey(key)
            for i in range(nvalues):
                name, value, kind = winreg.EnumValue(key, i)
                values.append((name, kind, value.hex() if isinstance(value, bytes) else value))
            for i in range(nkeys):
                name = winreg.EnumKey(key, i)
                with winreg.OpenKey(key, name) as child:
                    children[name] = walk(child)
            return {'values': sorted(values), 'children': children}
        for hive_name, hive in [('HKCU', winreg.HKEY_CURRENT_USER), ('HKLM', winreg.HKEY_LOCAL_MACHINE)]:
            for view in (winreg.KEY_WOW64_32KEY, winreg.KEY_WOW64_64KEY):
                label = f'{hive_name}-RFStateSide-{view}'
                try:
                    with winreg.OpenKey(hive, r'Software\RFStateSide', 0, winreg.KEY_READ | view) as key:
                        payload = json.dumps(walk(key), sort_keys=True).encode()
                    result['registry'][label] = hashlib.sha256(payload).hexdigest()
                except FileNotFoundError:
                    result['registry'][label] = 'absent'
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('output', type=Path)
    parser.add_argument('--compare', type=Path)
    args = parser.parse_args()
    current = snapshot()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(current, indent=2, sort_keys=True)+'\n')
    if args.compare:
        if current != json.loads(args.compare.read_text()):
            raise SystemExit('FAIL: native settings or historical files changed')
        print('PASS: native settings and historical files preserved (hash-only comparison)')
    else:
        print('Recorded native settings and historical files (hashes only)')
