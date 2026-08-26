#!/usr/bin/env python3
from pathlib import Path
import argparse
ap=argparse.ArgumentParser()
ap.add_argument('--rom',required=True); ap.add_argument('--out',required=True); ap.add_argument('--incbin-path')
a=ap.parse_args()
rom=Path(a.rom); out=Path(a.out); size=rom.stat().st_size
inc=a.incbin_path or rom.as_posix()
out.write_text(f'''/* Generated target ROM blob: avoids a multi-megabyte C initializer. */
.section .rodata_binf,"a"
.balign 4
.global rom_size
.type rom_size,@object
rom_size:
    .long {size}
.size rom_size, 4

.balign 2
.global rom_data
.type rom_data,@object
rom_data:
    .incbin "{inc}"
.size rom_data, .-rom_data
''')
print(f'wrote {out} for {rom} ({size} bytes)')
