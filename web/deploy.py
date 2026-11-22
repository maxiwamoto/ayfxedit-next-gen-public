"""Copy only browser runtime files into a local site directory (no deletion)."""
from pathlib import Path
import shutil,sys,hashlib,re
source=Path(__file__).resolve().parent
target=Path(sys.argv[1]).resolve()
target.mkdir(parents=True,exist_ok=True)
for name in ['.htaccess','index.html','studio.css','app.js','advanced.js','workshop.js','help.js','model.js','client.js','worker.js','CREDITS.md','THIRD_PARTY_NOTICES.md']:
 shutil.copyfile(source/name,target/name)
for folder in ['dist','vendor']:
 shutil.copytree(source/folder,target/folder,dirs_exist_ok=True)
# A single content revision keeps the entire module/worker/WASM graph in sync.
revision=hashlib.sha256(b''.join(p.read_bytes() for p in sorted(source.glob('*.js')))+ (source/'dist/ayfx-engine.wasm').read_bytes()).hexdigest()[:12]
for p in [*target.glob('*.js'), target/'dist/ayfx-engine.mjs']:
 text=p.read_text(encoding='utf-8')
 text=re.sub(r"(\./[\w/-]+\.(?:js|mjs))(?=['\"])",lambda m:m[1]+'?v='+revision,text)
 if p.suffix=='.mjs': text=text.replace('ayfx-engine.wasm','ayfx-engine.wasm?v='+revision)
 p.write_text(text,encoding='utf-8')
p=target/'index.html';p.write_text(p.read_text(encoding='utf-8').replace('src="app.js"','src="app.js?v='+revision+'"'),encoding='utf-8')
print('Browser editor deployed to',target,'revision',revision)
