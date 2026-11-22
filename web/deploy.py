"""Copy only browser runtime files into a local site directory (no deletion)."""
from pathlib import Path
import shutil,sys
source=Path(__file__).resolve().parent
target=Path(sys.argv[1]).resolve()
target.mkdir(parents=True,exist_ok=True)
for name in ['.htaccess','index.html','studio.css','app.js','help.js','model.js','client.js','worker.js','CREDITS.md','THIRD_PARTY_NOTICES.md']:
 shutil.copyfile(source/name,target/name)
for folder in ['dist','vendor']:
 shutil.copytree(source/folder,target/folder,dirs_exist_ok=True)
print('Browser editor deployed to',target)
