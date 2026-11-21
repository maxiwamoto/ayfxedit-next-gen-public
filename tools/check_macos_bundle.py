"""Reject bundled Mach-O binaries which still require Homebrew/build-tree files."""
import pathlib
import subprocess
import sys
app = pathlib.Path(sys.argv[1])
errors = []
count = 0
for path in app.rglob('*'):
    if not path.is_file() or path.is_symlink():
        continue
    kind = subprocess.check_output(['file', '-b', str(path)], text=True)
    if 'Mach-O' not in kind:
        continue
    count += 1
    # LC_ID_DYLIB identifies this binary; it is not a dependency to load.
    identities = subprocess.check_output(['otool', '-D', str(path)], text=True).splitlines()[1:]
    lines = subprocess.check_output(['otool', '-L', str(path)], text=True).splitlines()[1:]
    for line in lines:
        dep = line.strip().split(' (compatibility')[0]
        if dep in identities:
            continue
        if dep.startswith('/') and not dep.startswith(('/System/Library/', '/usr/lib/')):
            errors.append(f'{path}: external dependency {dep}')
if errors:
    raise SystemExit('\n'.join(errors))
if not count:
    raise SystemExit('No Mach-O files found')
print(f'PASS: {count} Mach-O files have no external non-system absolute dependencies')
