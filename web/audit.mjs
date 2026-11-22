import assert from 'node:assert/strict';
import fs from 'node:fs';import path from 'node:path';import {spawnSync} from 'node:child_process';
import createEngine from './dist/ayfx-engine.mjs';import * as M from './model.js';import {unzipSync,zipSync} from './vendor/fflate.js';
const engine=await createEngine();const call=(op,args)=>{const r=engine.run(op,args);if(!r.ok)throw Error(r.error);return r.data;};
const root=path.resolve('native/build/web-audit-'+Date.now());fs.mkdirSync(root,{recursive:true});
const verifier=path.resolve(process.env.WEB_NATIVE_VERIFY || 'native/build/web-audit-native/Release/web-native-verify.exe');
const bytes=p=>new Uint8Array(fs.readFileSync(p));let wavCount=0,streamCount=0,sdkCount=0,groupCount=0;const reports=[];
function write(p,b){fs.mkdirSync(path.dirname(p),{recursive:true});fs.writeFileSync(p,b);}
function compare(a,b,label){assert.deepEqual(Buffer.from(a),Buffer.from(b),label);}
function verify(doc,name){
 const dir=path.join(root,name);fs.mkdirSync(dir);const s=call('save',{doc,group:0});write(path.join(dir,'bank.afb'),s.bank);write(path.join(dir,'bank.afb.ayfx-project'),s.project);
 const loaded=call('decode',{bytes:s.bank,side:s.project,extension:'afb'});const again=call('save',{doc:loaded,group:0});compare(again.bank,s.bank,'roundtrip bank');compare(again.project,s.project,'roundtrip metadata/source');
 const zip=zipSync({'bank.afb':s.bank,'bank.afb.ayfx-project':s.project});write(path.join(dir,'project.ayfx.zip'),zip);const unzipped=unzipSync(bytes(path.join(dir,'project.ayfx.zip')));compare(unzipped['bank.afb'],s.bank,'disk ZIP');
 const native=path.join(dir,'native');const run=spawnSync(verifier,[path.join(dir,'bank.afb'),native],{encoding:'utf8'});assert.equal(run.status,0,run.stdout+run.stderr);compare(bytes(path.join(native,'roundtrip.afb')),s.bank,'native bank');compare(bytes(path.join(native,'roundtrip.afb.ayfx-project')),s.project,'native companion');
 const channel=doc.groups[0].tracks.findIndex(t=>t>=0),t=doc.groups[0].tracks[channel];
 const curve=call('curve',{doc,group:0,channel,first:2,last:18,lane:1,smooth:true,points:[{x:0,y:.15},{x:.3,y:.8},{x:.65,y:.1},{x:1,y:.5}]});
 const c=doc.tracks[t].cells[2]||M.cell();const pitch=call('envelope',{doc,group:0,channel,first:2,last:18,enabled:!!c[5],period:c[6],shape:0,trigger:0,transpose:3.5,depth:1.25,cycles:3});
 const seed=call('generate',{name:'Seed check',preset:1,frames:35,channels:3,clock:doc.clock,rate:doc.rate,startHz:500,endHz:70,noise:.6,seed:782});
 for(const [kind,result] of Object.entries({curve,pitch,seed})){const saved=call('save',{doc:result,group:0});compare(saved.bank,bytes(path.join(native,kind+'.afb')),'native '+kind);compare(saved.project,bytes(path.join(native,kind+'.project')),'native '+kind+' companion');}
 for(let g=0;g<doc.groups.length;g++){
  groupCount++;const args={doc,group:g};const regs=call('registers',args);compare(regs.data,bytes(path.join(native,g+'.ayregs')),'native AY14');assert.equal(regs.metadata,fs.readFileSync(path.join(native,g+'.json'),'utf8'));
  compare(call('batchWav',args),bytes(path.join(native,g+'.batch.wav')),'native batch WAV');
  for(let mask=0;mask<8;mask++){
   const wav=call('render',{...args,mask});compare(wav,bytes(path.join(native,g+'-'+mask+'.wav')),'native WAV mask '+mask);wavCount++;const view=new DataView(wav.buffer,wav.byteOffset,wav.byteLength);assert.equal(view.getUint32(24,true),44100);assert.equal(view.getUint32(40,true),wav.length-44);if(mask===0)assert.ok(wav.slice(44).every(v=>v===0));
  }
  call('streamStart',{...args,mask:7,from:0});let parts=[],count=0;for(const size of [1,16,1024,7,8192,11025,12810]){const c=call('streamNext',{count:size});parts.push(c.samples);count+=c.samples.length;if(c.ended)break;}const joined=new Float32Array(count);let at=0;for(const p of parts){joined.set(p,at);at+=p.length;}compare(new Uint8Array(joined.buffer),bytes(path.join(native,g+'.f32')).subarray(0,count*4),'native persistent stream');streamCount++;
  call('streamStart',{...args,mask:7,from:0});const one=call('streamNext',{count}).samples;compare(new Uint8Array(joined.buffer),new Uint8Array(one.buffer),'partition-independent stream');call('stop',{});
  for(let ch=0;ch<3;ch++){const file=path.join(native,g+'-'+ch+'.afx');if(fs.existsSync(file)){compare(call('classic',{...args,channel:ch}),bytes(file),'native AFX');for(const kind of ['csv','vt2'])compare(call(kind,{...args,channel:ch}),bytes(path.join(native,g+'-'+ch+'.'+kind)),'native '+kind);}}
  if(fs.existsSync(path.join(native,g+'.psg11'))){const r=call('registers',{...args,psg11:true});compare(r.data,bytes(path.join(native,g+'.psg11')),'native PSG11');assert.equal(r.metadata,fs.readFileSync(path.join(native,g+'.psg11.json'),'utf8'));}
  if(fs.existsSync(path.join(native,g+'.legacy.wav')))compare(call('legacy',{...args,mask:7,from:0}),bytes(path.join(native,g+'.legacy.wav')),'native legacy WAV');
 }
 const sdk=call('sdk',{doc,group:0});for(const [name,data] of Object.entries(sdk)){compare(data,bytes(path.join(native,'sdk',name)),'native SDK '+name);sdkCount++;}
 reports.push({name,groups:doc.groups.length,tracks:doc.tracks.length,embeddedAudioBytes:doc.tracks.reduce((n,t)=>n+t.sourceAudio.length,0)});return dir;
}
if(process.argv[2]){const archive=unzipSync(bytes(process.argv[2]));const name=Object.keys(archive).find(n=>n.endsWith('.afb'));assert.ok(name);const doc=call('decode',{bytes:archive[name],side:archive[name+'.ayfx-project'],extension:'afb'});verify(doc,'downloaded-user-project');console.log('Downloaded project:',doc.groups.length,'groups;',doc.tracks.length,'tracks;',doc.tracks.reduce((n,t)=>n+t.sourceAudio.length,0),'embedded audio bytes preserved.');}
for(const clock of [1789773,1773400,2000000])for(const rate of [50,60]){
 let doc=call('generate',{name:'Tone',preset:0,frames:24,channels:1,clock,rate,startHz:1200,endHz:100,noise:0});
 M.append(doc,call('generate',{name:'Noise',preset:1,frames:26,channels:3,clock,rate,startHz:500,endHz:50,noise:.7}));M.setLoop(doc,1,4,22);
 let env=M.empty();env.clock=clock;env.rate=rate;env.groups[0].name='Envelope';for(let f=0;f<20;f++)M.setCell(env,0,0,f,[254,0,15,1,0,1,700,f%5===0?10:-1]);M.setLoop(env,0,3,19);M.append(doc,env);
 verify(doc,clock+'-'+rate);
}
const report={wavCount,streamCount,sdkCount,groupCount,cases:reports};fs.writeFileSync(path.join(root,'report.json'),JSON.stringify(report,null,2));console.log('PASS',JSON.stringify({wavCount,streamCount,sdkCount,groupCount,root}));
