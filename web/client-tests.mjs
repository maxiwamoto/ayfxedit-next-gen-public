import assert from 'node:assert/strict';
import {Player} from './client.js';
import {topics} from './help.js';
const realSet=globalThis.setTimeout,realClear=globalThis.clearTimeout;let timers=new Map(),nextId=0,ctx;
globalThis.setTimeout=(fn,delay)=>{const id=++nextId;timers.set(id,{fn,delay});return id;};globalThis.clearTimeout=id=>timers.delete(id);
class AudioContextMock{
 constructor(){ctx=this;this.currentTime=0;this.destination={};this.sources=[];this.slow=false;}
 async resume(){}
 createBuffer(channels,n,rate){return{length:n,sampleRate:rate,copyToChannel(samples,ch){assert.equal(samples.length,n);assert.equal(ch,0);}};}
 createBufferSource(){const s={connect(){},start(at,offset){s.at=at;s.offset=offset;},stop(){s.stopped=true;}};this.sources.push(s);return s;}
}
globalThis.AudioContext=AudioContextMock;
try{
 let state=[],calls=0;const engine={async call(op){if(op==='streamStart')return{from:0};calls++;return{samples:new Float32Array(11025),ended:calls===3};}};
 const p=new Player(engine,(playing,error)=>state.push({playing,error}));await p.play({},0);assert.equal(calls,3);assert.equal(ctx.sources.length,3);assert.equal(ctx.sources[0].at,.08);assert.equal(ctx.sources[1].at,.33);assert.equal(ctx.sources[2].at,.5800000000000001);assert.ok([...timers.values()].some(t=>t.delay>700));const end=[...timers.values()][0];end.fn();assert.equal(p.playing,false);assert.ok(ctx.sources.every(s=>s.stopped));
 timers.clear();state=[];const underrun=new Player({async call(op){if(op==='streamStart')return{from:0};ctx.currentTime=2;return{samples:new Float32Array(11025),ended:false};}},(playing,error)=>state.push({playing,error}));await underrun.play({},0);assert.equal(underrun.playing,false);assert.match(state.find(x=>x.error).error.message,/could not keep up/);assert.equal(timers.size,0);
 let resolveStart;state=[];const pending=new Player({call(){return new Promise(resolve=>resolveStart=resolve);}},(playing,error)=>state.push({playing,error}));const run=pending.play({},0);await Promise.resolve();await Promise.resolve();assert.ok(resolveStart);pending.stop();resolveStart({from:0});await run;assert.equal(pending.playing,false);assert.equal(ctx.sources.length,0);
 const ab=new Player({},()=>{});await ab.audio(new Float32Array(44100),44100,.35);assert.equal(ctx.sources[0].offset,.35);ctx.currentTime=.2;assert.ok(Math.abs(ab.position()-.55)<1e-8);const position=ab.position();await ab.audio(new Float32Array(44100),44100,position);assert.ok(Math.abs(ctx.sources.at(-1).offset-.55)<1e-8);ab.stop();assert.equal(ab.position(),0);
 const ids=topics.map(t=>t[0]);assert.equal(new Set(ids).size,topics.length);for(const t of topics){assert.ok(t[3].length>250);for(const link of t[3].matchAll(/data-topic="([^"]+)"/g))assert.ok(ids.includes(link[1]));}
 console.log('PASS: scheduled audio continuity, finite stop, underrun reporting, cancellation race, and '+topics.length+' guide topics/links.');
}finally{globalThis.setTimeout=realSet;globalThis.clearTimeout=realClear;}
