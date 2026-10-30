# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
"""Read-only guard for the single-audit policy and new staged private artifacts."""
import json
from pathlib import Path
import re
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
snapshot = root / '.raynmea-source.json'
if snapshot.is_file():
    files = list(json.loads(snapshot.read_text(encoding='utf-8'))['source_files'])
    added = []
else:
    def git(*args):
        return subprocess.check_output(['git', '-C', str(root), *args], text=True).splitlines()
    files = git('ls-files')
    added = git('diff', '--cached', '--name-only', '--diff-filter=A')
files = [name for name in files if (root / name).is_file()]
errors = []
for name in files:
    path = Path(name)
    if path.suffix.lower() not in ('.md', '.txt', '.csv', '.rst'):
        continue
    if name == 'PUBLIC_RELEASE_AUDIT.md':
        continue
    if ('audit' in path.name.lower() or 'audit' in [p.lower() for p in path.parts[:-1]]
            or path.name.upper().startswith('PUBLIC_RELEASE_')
            or re.match(r'M\d.*(?:Inventory|Matrix|Comparison|Backlog)', path.name, re.I)):
        errors.append('Redundant audit document: ' + name)
if 'PUBLIC_RELEASE_AUDIT.md' not in files:
    errors.append('Authoritative audit is not registered in source')
else:
    audit = (root / 'PUBLIC_RELEASE_AUDIT.md').read_text(encoding='utf-8')
    for marker in ('PRIVATE', 'NO-GO', 'Historical artwork', 'corresponding-source'):
        if marker not in audit:
            errors.append('Missing release gate: ' + marker)
for name in added:
    p = Path(name)
    if (p.parts[0].lower() in ('build', 'private-profiles', 'private-captures')
            or p.suffix.lower() in ('.exe', '.dll', '.zip', '.gz', '.7z', '.user', '.key')
            or p.name.startswith('.env')):
        errors.append('Private/generated artifact newly staged: ' + name)
print(json.dumps({'authoritative_audit': 'PUBLIC_RELEASE_AUDIT.md',
                  'source_files_checked': len(files), 'errors': errors}, indent=2))
sys.exit(bool(errors))
