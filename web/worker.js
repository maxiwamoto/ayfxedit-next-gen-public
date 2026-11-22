import createEngine from './dist/ayfx-engine.mjs';
let engine;
const ready=createEngine().then(m=>{engine=m;postMessage({ready:true});}).catch(e=>postMessage({fatal:e.message}));
onmessage=async({data})=>{try{await ready;if(!engine)throw Error('The audio engine could not load.');const r=engine.run(data.op,data.args);postMessage({id:data.id,...r});}catch(e){postMessage({id:data.id,ok:false,error:e.message||String(e)});}};
