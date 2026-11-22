export const MAX_FRAMES=4096,MAX_TRACKS=256;
export const cell=()=>[0,0,0,0,0,0,1,-1];
export const track=(name='Untitled')=>({name,cells:[],tags:'',favorite:false,sourcePath:'',recipe:'',sourceAudio:new Uint8Array()});
export const group=(name='Untitled',tracks=[0,-1,-1])=>({name,tracks,loopStart:-1,loopEnd:-1,priority:128});
export const empty=()=>({tracks:[track()],groups:[group()],clock:1789773,rate:50});
export const clone=value=>structuredClone(value);
export function getCell(doc,g,ch,f){const t=doc.groups[g].tracks[ch];return t<0?cell():(doc.tracks[t].cells[f]||cell());}
export function setCell(doc,g,ch,f,value){if(f<0||f>=MAX_FRAMES)throw Error('Frame must be between 0 and 4095.');const t=doc.groups[g].tracks[ch];if(t<0)throw Error('Add this channel first.');const list=doc.tracks[t].cells;while(list.length<=f)list.push(cell());list[f]=[...value];}
export function duration(doc,g){let n=1;for(const t of doc.groups[g].tracks)if(t>=0){const c=doc.tracks[t].cells;for(let f=c.length-1;f>=0;f--)if(c[f][2]||c[f][5]){n=Math.max(n,f+1);break;}}return Math.max(n,doc.groups[g].loopEnd);}
export function append(doc,other){if(doc.tracks.length+other.tracks.length>MAX_TRACKS)throw Error('A bank can contain up to 256 tracks.');const offset=doc.tracks.length;doc.tracks.push(...clone(other.tracks));doc.groups.push(...other.groups.map(g=>({...clone(g),tracks:g.tracks.map(t=>t<0?-1:t+offset)})));return doc.groups.length-other.groups.length;}
export function removeGroup(doc,index){if(doc.groups.length===1)throw Error('Keep at least one effect in the bank.');const removed=new Set(doc.groups[index].tracks.filter(t=>t>=0));doc.groups.splice(index,1);const map=new Map();doc.tracks=doc.tracks.filter((t,i)=>{if(removed.has(i))return false;map.set(i,map.size);return true;});for(const g of doc.groups)g.tracks=g.tracks.map(t=>t<0?-1:map.get(t));}
export function removeChannel(doc,g,ch){const target=doc.groups[g];if(target.tracks.filter(t=>t>=0).length===1){if(doc.groups.length===1)throw Error('Keep at least one track in the bank.');removeGroup(doc,g);return;}const t=target.tracks[ch];if(t<0)return;target.tracks[ch]=-1;doc.tracks.splice(t,1);for(const x of doc.groups)x.tracks=x.tracks.map(i=>i>t?i-1:i);}
export function addChannel(doc,g,ch){if(doc.tracks.length>=MAX_TRACKS)throw Error('The bank already contains 256 tracks.');if(doc.groups[g].tracks[ch]>=0)return;doc.groups[g].tracks[ch]=doc.tracks.length;doc.tracks.push(track(doc.groups[g].name+' '+String.fromCharCode(65+ch)));}
export function editRows(doc,g,ch,first,last,action,linked=true,clipboard=null){
 const count=last-first+1;if(first<0||last>=MAX_FRAMES||count<1)throw Error('Invalid frame selection.');const target=doc.groups[g];const affected=target.tracks.map((t,c)=>({t,c})).filter(x=>x.t>=0&&(linked||x.c===ch));
 if(action==='copy')return target.tracks.map((t,c)=>t<0||(!linked&&c!==ch)?[]:Array.from({length:count},(_,i)=>[...getCell(doc,g,c,first+i)]));
 if(action==='insert'||action==='duplicate'||action==='paste')for(const {t} of affected){const cells=doc.tracks[t].cells;if(cells.slice(MAX_FRAMES-count).some(c=>c.some((v,i)=>v!==cell()[i])))throw Error('Insertion would discard frames at the end. Shorten the effect first.');}
 for(const {t,c} of affected){const list=doc.tracks[t].cells;while(list.length<=last)list.push(cell());
  if(action==='clear'){for(let f=first;f<=last;f++)list[f]=cell();continue;}
  if(action==='delete')list.splice(first,count);
  else{let values=Array.from({length:count},()=>cell());if(action==='duplicate')values=list.slice(first,last+1).map(x=>[...x]);if(action==='paste')values=Array.from({length:count},(_,i)=>[...(clipboard[c]?.[i]||cell())]);list.splice(first,0,...values);list.length=Math.min(list.length,MAX_FRAMES);}
 }
 if(linked&&target.loopStart>=0&&action!=='clear'){
  const adjust=p=>action==='delete'?(p<=first?p:Math.max(first,p-count)):(p>=first?Math.min(MAX_FRAMES,p+count):p);
  target.loopStart=adjust(target.loopStart);target.loopEnd=adjust(target.loopEnd);if(target.loopEnd<=target.loopStart)target.loopStart=target.loopEnd=-1;
 }
 return null;
}
export function setLoop(doc,g,a,b){if(a===-1&&b===-1){doc.groups[g].loopStart=doc.groups[g].loopEnd=-1;return;}if(!Number.isInteger(a)||!Number.isInteger(b)||a<0||b<=a||b>MAX_FRAMES)throw Error('Loop end must be after its start, within 4096 frames.');doc.groups[g].loopStart=a;doc.groups[g].loopEnd=b;}
export function applyCurve(doc,g,ch,first,last,lane,start,end){const max=[4095,31,15][lane];for(let f=first;f<=last;f++){const c=[...getCell(doc,g,ch,f)];c[lane]=Math.round(start+(end-start)*(last===first?0:(f-first)/(last-first)));c[lane]=Math.max(0,Math.min(max,c[lane]));if(lane===2)c[5]=0;else c[lane===0?3:4]=1;setCell(doc,g,ch,f,c);}}

export function trackLength(t){for(let f=t.cells.length-1;f>=0;f--)if(t.cells[f][2]||t.cells[f][5])return f+1;return 0;}
export function conflicts(doc,g,frame){let noise=null,period=null,shape=null;const errors=[];for(const t of doc.groups[g].tracks){if(t<0)continue;const c=doc.tracks[t].cells[frame]||cell();if(c[4]&&(c[2]||c[5])){if(noise!==null&&noise!==c[1])errors.push('Shared noise conflict');noise=c[1];}if(c[5]){if(period!==null&&period!==c[6])errors.push('Shared envelope period conflict');period=c[6];}if(c[7]>=0){if(shape!==null&&shape!==c[7])errors.push('Shared envelope shape conflict');shape=c[7];}}return [...new Set(errors)];}
