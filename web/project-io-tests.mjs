// Run the real application save handler with deferred serialization/storage.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
const source=fs.readFileSync(new URL('./app.js',import.meta.url),'utf8');
const handler=source.slice(source.indexOf('async function saveProject()'),source.indexOf('async function openFiles('));
function setup(){
 let finish;const deletes=[],downloads=[],messages=[],autosave={};
 const c={doc:{tracks:[{name:'Before'}]},revision:1,active:0,dirty:true,filename:'Project',saveTimer:3,
  player:{stop(){}},M:{clone:structuredClone},cleanName:x=>x,zipSync:x=>x,
  engine:{call:()=>new Promise(resolve=>finish=resolve)},download:(...args)=>downloads.push(args),
  recovery:async action=>deletes.push(action),clearTimeout(){},status:m=>messages.push(m),$:()=>autosave};
 vm.createContext(c);vm.runInContext(handler,c);
 return {c,deletes,downloads,messages,autosave,finish:()=>finish({bank:[],project:[]})};
}
let t=setup(),p=t.c.saveProject();t.finish();await p;assert.equal(t.c.dirty,false);assert.deepEqual(t.deletes,['delete']);assert.equal(t.downloads.length,1);
t=setup();p=t.c.saveProject();t.c.doc.tracks[0].name='Later';t.c.revision++;t.finish();await p;assert.equal(t.c.dirty,true);assert.equal(t.deletes.length,0);
t=setup();p=t.c.saveProject();t.c.doc={tracks:[]};t.finish();await p;assert.equal(t.c.dirty,true);assert.equal(t.deletes.length,0);
t=setup();t.c.recovery=async()=>{throw Error('Storage unavailable');};p=t.c.saveProject();t.finish();await p;assert.equal(t.downloads.length,1);assert.match(t.autosave.textContent,/could not be cleared/);assert.match(t.messages.at(-1),/ZIP prepared/);
console.log('PASS: saving preserves newer edits/reopened projects and completes downloads despite recovery cleanup errors.');
