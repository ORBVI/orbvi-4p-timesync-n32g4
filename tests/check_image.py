#!/usr/bin/env python3
"""Fail build if vector or any HEX record enters forbidden Flash regions."""
import pathlib
import struct
import sys

elf, hexfile = (pathlib.Path(p) for p in sys.argv[1:])
assert elf.exists()
upper = 0
low = None
high = 0
vectors = {}
for line in hexfile.read_text().splitlines():
    b = bytes.fromhex(line[1:])
    assert b[0] == len(b) - 5 and sum(b) % 256 == 0
    n, address, kind = b[0], int.from_bytes(b[1:3], 'big'), b[3]
    if kind == 4:
        upper = int.from_bytes(b[4:6], 'big') << 16
    elif kind == 0:
        addr = upper + address
        low = addr if low is None else min(low, addr)
        high = max(high, addr + n)
        for offset, value in enumerate(b[4:4+n]):
            if addr + offset in range(0x08000000, 0x08000008):
                vectors[addr + offset] = value
assert low == 0x08000000, hex(low) if low else 'no data'
assert high <= 0x0807F800, hex(high)
sp, reset = struct.unpack('<II', bytes(vectors[0x08000000 + i] for i in range(8)))
assert sp == 0x20024000, hex(sp)
assert 0x08000000 <= (reset & ~1) < high and reset & 1, hex(reset)
print(f'Flash image OK: 0x{low:08X}..0x{high:08X}, SP=0x{sp:08X}, reset=0x{reset:08X}')
