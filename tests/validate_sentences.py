# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
from pathlib import Path
from functools import reduce
from operator import xor
import sys
p=Path(sys.argv[1]) if len(sys.argv)>1 else Path('seven-sentences.nmea')
raw=p.read_bytes()
expected={
'GGA':['123456.50','4230.000','S','07015.000','E','2','12','0.8','-10.5','M','-25.5','M','1.5','42'],
'GLL':['4230.000','S','07015.000','E','123456.50','A'],
'HDG':['123.4','2.5','W','10.5','E'],
'HDT':['123.4','T'],
'RMC':['123456.50','A','4230.000','S','07015.000','E','10.0','123.4','031024','10.5','W','S','S'],
'VTG':['123.4','T','120.0','M','10.0','N','18.5','K'],
'ZDA':['123456.50','13','10','2026','00','00'],
}
lines=raw.splitlines(keepends=True)
assert len(lines)==7
seen=set()
for line in lines:
 assert line.endswith(b'\r\n') and line.startswith(b'$')
 assert len(line)<=82
 body,checksum=line[1:-2].split(b'*')
 assert len(checksum)==2 and all(c in b'0123456789ABCDEF' for c in checksum)
 assert int(checksum,16)==reduce(xor,body,0)
 fields=body.decode('ascii').split(',')
 code=fields[0][2:]
 assert fields[0][:2]=='GP' and code not in seen
 assert fields[1:]==expected[code],(code,fields)
 seen.add(code)
 print(f'{code}: expected fields, ASCII, XOR checksum, CRLF and length PASS ({len(line)} bytes)')
assert seen==set(expected)
