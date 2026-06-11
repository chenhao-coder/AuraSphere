#!/usr/bin/env python3
"""Split firmware.bin into 2048-byte chunks and compute MD5 for each.

Usage:
  python chunk_md5.py firmware.bin > pc_chunks.txt

Then compare pc_chunks.txt against STM32 serial output.
"""

import hashlib
import sys

if len(sys.argv) < 2:
    print(f"Usage: {sys.argv[0]} <firmware.bin>", file=sys.stderr)
    sys.exit(1)

with open(sys.argv[1], 'rb') as f:
    data = f.read()

chunk_size = 2048
for offset in range(0, len(data), chunk_size):
    chunk = data[offset:offset+chunk_size]
    md5 = hashlib.md5(chunk).hexdigest()
    print(f"chunk@{offset} md5={md5}")

with open('.\Tools\cmq3a2lxz00003j9wqtrwa79m.bin','rb') as f: data=f.read()
for o in range(0,151060,2048):
    sz=min(2048,len(data)-o)
    print(f"chunk@{o} mid:{data[o+1024:o+1028].hex()} tail:{data[o+sz-4:o+sz].hex()}")

