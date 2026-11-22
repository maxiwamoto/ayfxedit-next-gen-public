import assert from 'node:assert/strict';
import {Engine} from './client.js';
let latest;
class WorkerMock {
 constructor(){latest=this;this.sent=[];}
 postMessage(message){this.sent.push(message);}
 terminate(){this.terminated=true;}
 ready(){this.onmessage({data:{ready:true}});}
}
globalThis.Worker=WorkerMock;
// Closing before startup, after ready, or between call() and its await must settle.
let e=new Engine(),p=e.call('decode',{});e.close();await assert.rejects(p,/Cancelled/);
e=new Engine();latest.ready();await e.ready;e.close();await assert.rejects(e.call('decode',{}),/Cancelled/);
e=new Engine();latest.ready();await e.ready;p=e.call('decode',{});e.close();await assert.rejects(p,/Cancelled/);assert.equal(latest.sent.length,0);
e=new Engine();latest.ready();p=e.call('render',{});await Promise.resolve();assert.equal(latest.sent.length,1);e.close();await assert.rejects(p,/Cancelled/);assert.equal(e.pending.size,0);
console.log('PASS: worker cancellation settles before startup, after readiness, at await boundary, and during work.');
