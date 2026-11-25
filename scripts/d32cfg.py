#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Show or set DOS/32A's built-in configuration (the ID32 header in its
stub) in an executable, as SS/32A's menus would.

Usage: d32cfg.py <exe> [--load 1|2|3|4] [--extmem <KB>|max] [--dosbuf <bytes>]

--load is the Object Loading Scheme: 1 loads all objects into conventional
memory first and the rest high, 2 the 16-bit ones low, 3 all low or fail,
4 (the default) all high, as DOS/4GW. A 512 KB TS-3100 has no extended
memory, so it needs 1 or 3. --extmem caps the extended memory DOS/32A
takes. The layout is struct header in DOS/32A's src/ss/main.h, and the
scheme is bits 5:4 of dos32a_misc (menus.c).
"""
import struct
import sys

FMT = "<4sBBBBHBBHHIBBHH"
NAMES = ["id", "kernel_misc", "kernel_pagetables", "kernel_phystables", "kernel_callbacks", "kernel_selectors",
         "kernel_rmstacks", "kernel_pmstacks", "kernel_rmstacklen", "kernel_pmstacklen", "kernel_maxextmem",
         "dos32a_misc", "dos32a_misc2", "dos32a_lowbufsize", "dos32a_version"]

args = sys.argv[1:]
path = args.pop(0)
data = bytearray(open(path, "rb").read())
at = data.find(b"ID32")
if at < 0:
    raise SystemExit(f"{path}: no DOS/32A header (ID32)")
h = dict(zip(NAMES, struct.unpack_from(FMT, data, at)))
if h["dos32a_version"] != 0x090C:
    raise SystemExit(f"{path}: DOS/32A version {h['dos32a_version']:#x}, not 9.1.2 (0x90c)")

while args:
    a = args.pop(0)
    v = args.pop(0)
    if a == "--load":
        h["dos32a_misc"] = (h["dos32a_misc"] & 0xCF) | ((int(v) - 1) << 4)
    elif a == "--extmem":
        h["kernel_maxextmem"] = 0xFFFFFFFF if v == "max" else int(v) * 1024
    elif a == "--dosbuf":
        h["dos32a_lowbufsize"] = int(v) // 16
    else:
        raise SystemExit(f"unknown option {a}")
struct.pack_into(FMT, data, at, *[h[n] for n in NAMES])
open(path, "wb").write(data)

print(f"{path}: object loading scheme #{((h['dos32a_misc'] >> 4) & 3) + 1}, "
      f"max. extended memory {'all' if h['kernel_maxextmem'] == 0xFFFFFFFF else str(h['kernel_maxextmem'] // 1024) + ' KB'}, "
      f"DOS buffer {h['dos32a_lowbufsize'] * 16} bytes")
