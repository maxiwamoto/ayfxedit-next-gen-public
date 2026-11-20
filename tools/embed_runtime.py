"""Embed the checked player sources so a standalone editor can export the SDK."""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
files = [p for p in (root / 'runtime').iterdir() if p.suffix in ('.asm', '.c', '.h', '.bat', '.md')]
out = '#pragma once\nstruct RuntimeFile { const char* name; const char* text; };\ninline const RuntimeFile runtimeFiles[]={\n'
for p in sorted(files):
    text = p.read_text(encoding='utf-8-sig')
    assert ')AYFXSDK"' not in text
    out += '{"' + p.name + '",R"AYFXSDK(' + text + ')AYFXSDK"},\n'
out += '};\n'
(root / 'native/build').mkdir(exist_ok=True)
(root / 'native/build/runtime_embedded.h').write_text(out, encoding='utf-8')
