#!/usr/bin/env python3
from pathlib import Path
import argparse
import re
import shutil

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--src', default=str(root / 'generated_upstream'))
parser.add_argument('--dst', default=str(root / 'generated_sgdk'))
parser.add_argument('--prefix', default='basicdemo')
args = parser.parse_args()

src = Path(args.src)
dst = Path(args.dst)
prefix = args.prefix
dst.mkdir(parents=True, exist_ok=True)

for old_file in dst.glob(f'{prefix}_*.c'):
    old_file.unlink()
for name in [f'{prefix}.h', f'{prefix}_internal.h', f'{prefix}_rom.c']:
    shutil.copy2(src / name, dst / name)
for pattern in [f'{prefix}_funcs_*.c', f'{prefix}_dispatch_chunk_*.c']:
    files = sorted(src.glob(pattern))
    if not files:
        raise SystemExit(f'missing generated files: {pattern}')
    for file in files:
        shutil.copy2(file, dst / file.name)

text = (src / f'{prefix}.c').read_text()
text = text.replace('#include <stdio.h>\n', '').replace('#include <stdlib.h>\n', '')
text = re.sub(r'ctx->mbc_type\s*=\s*rom_data\[0x147\];',
              'ctx->mbc_type = gbrt_sgdk_rom_read8(ctx, 0x147u);', text)
pattern = re.compile(
    r'\s*if \(gbrt_instruction_limit > 0\) \{.*?\n\s*\}\n'
    r'\s*if \(ctx->trace_entries_enabled\) gbrt_log_trace\(ctx, bank, addr\);\n'
    r'\s*if \(gbrt_trace_enabled\) \{.*?\n\s*\}\n',
    re.S,
)
text, n = pattern.subn('\n', text, count=1)
if n != 1:
    raise SystemExit(f'failed to strip desktop diagnostics from {prefix}.c')
(dst / f'{prefix}.c').write_text(text)
print(f'prepared {dst} ({prefix})')
