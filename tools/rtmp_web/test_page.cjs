// Source-level UI state tests with a fake DOM and fake camera API, no browser IO.
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const html=fs.readFileSync(__dirname+'/index-v20.html','utf8');
const elems={};for(const [,id] of html.matchAll(/id="([^"]+)"/g))elems[id]={value:id==='rate'?'2000':'',disabled:false,textContent:'',className:''};
const labels=[...html.matchAll(/data-i18n="([^"]+)"/g)].map(([,key])=>({dataset:{i18n:key},textContent:''}));
const calls=[],storage={},doc={documentElement:{},hidden:false,getElementById:id=>elems[id],querySelectorAll:()=>labels};
let enabled=true,error='';
const ctx=vm.createContext({document:doc,navigator:{language:'en-US'},localStorage:{getItem:k=>storage[k],setItem:(k,v)=>storage[k]=v},URLSearchParams,AbortController,Date,setTimeout,clearTimeout,setInterval:()=>0,
 fetch:async(url,opt)=>{const params=opt.body?Object.fromEntries(opt.body):null;calls.push([url,params]);
  if(error)return{json:async()=>({ok:false,code:error})};
  if(url.endsWith('network')){if(params)enabled=params.action==='enable';return{json:async()=>({ok:true,enabled,active:'sta',pending:!enabled,card:true,ip:'192.168.100.117'})};}
  if(!params)return{json:async()=>({ok:true,token:'a'.repeat(64),url:'rtmp://192.168.100.6:1935/live/osmo',bitrate:2000})};
  return{json:async()=>({ok:true,streaming:params.action==='start',elapsed:2,bitrate:2000})};
 }});
vm.runInContext(html.match(/<script>([\s\S]*)<\/script>/)[1],ctx);
(async()=>{
 await new Promise(r=>setImmediate(r));
 assert.equal(doc.documentElement.lang,'en');assert.equal(elems['stream-state'].textContent,'Stopped');
 assert(!calls.some(([,p])=>p?.action==='start'));
 assert.equal(elems.start.disabled,false);assert.equal(elems.sta.value,'on');
 elems.url.value='rtmp://example/live/test';elems.url.oninput();assert(elems.start.disabled);
 await elems.save.onclick();assert.equal(elems.start.disabled,false);
 await elems.start.onclick();assert.equal(elems['stream-state'].textContent,'Streaming');
 elems.sta.value='off';await elems['network-save'].onclick();assert.equal(elems['network-owner'].textContent,'Custom STA');assert(elems.pending.textContent.includes('power-off'));
 elems.language.value='zh-CN';elems.language.onchange();assert.equal(storage['oa.language'],'zh-CN');assert.equal(elems['network-owner'].textContent,'自定义 STA');
 error='timeout';await elems.refresh.onclick();assert.equal(elems['stream-state'].textContent,'尚未确认');assert(elems['stream-msg'].textContent.includes('未确认命令'));
 elems.language.value='en';elems.language.onchange();assert(elems['stream-msg'].textContent.includes('did not confirm'));
 vm.runInContext('if(JSON.stringify(Object.keys(strings.en).sort())!==JSON.stringify(Object.keys(strings["zh-CN"]).sort()))throw Error("translation mismatch")',ctx);
 assert(!/usb|192\.168\.77/i.test(html));assert(!html.includes('/cgi-bin/apply'));
 console.log('PASS: translated states/errors, language persistence, manual streaming, dirty config, STA pending switch, retired UI removed');
})().catch(e=>{console.error(e);process.exitCode=1});
