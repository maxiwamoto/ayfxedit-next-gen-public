"""Format maintained tniASM sources using the project hexadecimal/tab style.

Quoted strings, symbol spelling and comments are preserved. Upstream references
are outside the search roots. --check is read-only; generators use format_text.
"""
import argparse
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
TOKEN = re.compile(r'''("[^"\n]*"|'[^'\n]*')|(?<![\w.])([0-9A-Fa-f]+[hH]|[0-9]+)(?![\w.])''')


def format_text(text):
    result = []
    equ_width = max([len(m[1])+1 for line in text.splitlines()
                     if (m := re.match(r'^([\w.]+):?\s+EQU\b', line.strip(), re.I))] or [0])+2
    for line in text.splitlines():
        # A semicolon inside a quoted filename/text is not a comment delimiter.
        quote, split = None, len(line)
        for i,c in enumerate(line):
            if c in '\"\'':
                if quote == c: quote = None
                elif quote is None: quote = c
            if c == ';' and quote is None:
                split = i; break
        code, comment = line[:split].strip(), line[split:].strip()
        if not code:
            result.append(comment)
            continue
        mnemonic = code.split()[0].upper()
        word = (mnemonic in ('DW','ORG','PHASE') or
                bool(re.fullmatch(r'LD\s+(?:BC|DE|HL|IX|IY|SP),(?:[0-9A-F]+h|[0-9]+)',code,re.I)))
        def number(match):
            if match[1]: return match[1]
            token = match[2]
            value = int(token[:-1],16) if token[-1:] in ('h','H') else int(token)
            explicit_word = token[-1:] in ('h','H') and len(token[:-1]) >= 4
            width = 4 if word or explicit_word or value>255 else 2
            return f'{value:0{width}X}h'
        code = TOKEN.sub(number,code)
        if m := re.match(r'^([\w.]+):?\s+EQU\s+(.+)$',code,re.I):
            code = (m[1]+':').ljust(equ_width)+'EQU\t'+m[2]
        elif code.endswith(':'):
            # Attach immediately preceding comment paragraphs to this label.
            insertion = len(result)
            while insertion and result[insertion-1].startswith(';'):
                insertion -= 1
            if insertion and result[insertion-1]: result.insert(insertion,'')
        else:
            parts = code.split(None,1)
            head = parts[0].lower() if parts[0].startswith('%') else parts[0].upper()
            code = '\t'+head+ ('\t'+parts[1] if len(parts)>1 else '')
        result.append(code+ ('\t'+comment if comment else ''))
    return re.sub(r'\n{3,}','\n\n','\n'.join(result)).strip('\n')+'\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check',action='store_true')
    args = parser.parse_args()
    paths = list((ROOT/'runtime').rglob('*.asm'))
    generated = ROOT/'native/build/sdk-test'
    paths += [generated/p.name for p in (ROOT/'runtime').glob('*.asm') if (generated/p.name).exists()]
    paths += [p for name in ('effects.asm', 'effect_ids.asm') if (p := generated/name).exists()]
    changed = []
    for path in sorted(paths):
        before = path.read_text()
        after = format_text(before)
        if before != after:
            changed.append(str(path.relative_to(ROOT)))
            if not args.check: path.write_text(after)
    if changed: print('\n'.join(changed))
    if args.check and changed: raise SystemExit(1)
    print(f'{len(paths)} ASM files checked' if args.check else f'{len(changed)} ASM files formatted')


if __name__ == '__main__': main()
