"""Build the shared AY engine for browsers; requires emsdk 7.0.1 or compatible."""
from pathlib import Path
import os,subprocess,shutil
root=Path(__file__).resolve().parents[1]
subprocess.run(['python',str(root/'tools/embed_runtime.py')],cwd=root,check=True)
compiler=os.environ.get('EMXX') or shutil.which('em++')
if not compiler:
 compiler=str(root/'native/build/emsdk/upstream/emscripten/em++.exe')
if (root/'native/build/emsdk/.emscripten').exists():
 os.environ.setdefault('EM_CONFIG',str(root/'native/build/emsdk/.emscripten'))
(root/'web/dist').mkdir(exist_ok=True)
subprocess.run([compiler,str(root/'web/bridge.cpp'),'-I'+str(root/'native'),'-std=c++17','-O2','--bind','-fexceptions','-sDISABLE_EXCEPTION_CATCHING=0','-sMODULARIZE=1','-sEXPORT_ES6=1','-sENVIRONMENT=web,worker,node','-sALLOW_MEMORY_GROWTH=1','-sSTACK_SIZE=16777216','-sINITIAL_MEMORY=33554432','-sMAXIMUM_MEMORY=1073741824','-o',str(root/'web/dist/ayfx-engine.mjs')],cwd=root,check=True)
shutil.copyfile(root/'web/node_modules/fflate/esm/browser.js',root/'web/vendor/fflate.js')
shutil.copyfile(root/'web/node_modules/fflate/LICENSE',root/'web/vendor/fflate-LICENSE.txt')
for name in ['CREDITS.md','THIRD_PARTY_NOTICES.md']:
 shutil.copyfile(root/name,root/'web'/name)
shutil.copyfile(Path(compiler).resolve().parent/'LICENSE',root/'web/vendor/emscripten-LICENSE.txt')
with (root/'web/THIRD_PARTY_NOTICES.md').open('a',encoding='utf-8') as f:
 f.write('\n## Web Studio\n\nThe browser build contains fflate (Arjun Barrett, MIT) and Emscripten runtime components (MIT/UIUC). Their complete notices are retained in vendor/fflate-LICENSE.txt and vendor/emscripten-LICENSE.txt. Qt is not used by the web build. The inherited editor attribution above remains applicable.\n')
print('Browser engine and third-party files built.')
