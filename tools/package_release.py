"""Package the built editor and committed public source; never include build caches."""
import argparse, hashlib, subprocess, zipfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser();ap.add_argument('--version',default='1.0.0');a=ap.parse_args()
assert all(c.isalnum() or c in '.-' for c in a.version)
out=root/'release';out.mkdir(exist_ok=True)
exe=root/'native/dist/ayfxedit-next-gen.exe';assert exe.is_file(),'Run build_native.bat first'
name=f'ayfxedit-next-gen-{a.version}'
portable=out/f'{name}-windows-x64.zip';source=out/f'{name}-source.zip'
with zipfile.ZipFile(portable,'w',zipfile.ZIP_DEFLATED) as z:
    z.write(exe,'ayfxedit-next-gen.exe')
    for p in ['README.md','CHANGELOG.md','CREDITS.md','THIRD_PARTY_NOTICES.md','native/README.md','runtime/README.md','docs/register-export.md','docs/VALIDATION.md']:
        z.write(root/p,p)
subprocess.run(['git','archive','--format=zip',f'--prefix={name}/',f'--output={source}','HEAD'],cwd=root,check=True)
with zipfile.ZipFile(source) as z:
    assert all('/.git/' not in p and '/native/build/' not in p and not p.endswith('.exe') for p in z.namelist())
with zipfile.ZipFile(portable) as z:assert z.read('ayfxedit-next-gen.exe')==exe.read_bytes()
(out/'SHA256SUMS.txt').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.name+'\n' for p in [portable,source,exe]),encoding='ascii')
for p in [portable,source,out/'SHA256SUMS.txt']:print(p.name,p.stat().st_size)
