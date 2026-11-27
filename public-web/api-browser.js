'use strict';
const el=id=>document.getElementById(id);let endpoints=[];
function redact(value){if(Array.isArray(value))return value.map(redact);if(value&&typeof value==='object')return Object.fromEntries(Object.entries(value).map(([key,item])=>[key,/token|secret|credential|csrf|password|(?:^|_)codes?$|device_code|recovery.*code/i.test(key)?'[redacted]':redact(item)]));return value;}
async function catalog(){const response=await fetch(kfxRoute('/api-catalog.json'));if(!response.ok)throw Error('Endpoint reference unavailable.');endpoints=await response.json();describe();}
function describe(){el('api-description').textContent=endpoints.find(item=>item.path===el('api-endpoint').value)?.description||'';}
el('api-endpoint').addEventListener('change',describe);
el('api-filter').addEventListener('input',event=>{const query=event.target.value.toLowerCase();document.querySelectorAll('#api-reference tbody tr').forEach(row=>row.hidden=!row.textContent.toLowerCase().includes(query));});
el('api-explorer').addEventListener('submit',async event=>{
 event.preventDefault();const path=el('api-endpoint').value;
 if(!endpoints.some(item=>item.path===path&&item.method==='GET'&&item.interactive)){el('api-notice').textContent='Choose a listed read endpoint.';return;}
 el('api-run').disabled=true;el('api-notice').textContent='';const start=performance.now();
 try{const response=await fetch(kfxRoute(path),{credentials:'same-origin',signal:AbortSignal.timeout(6000)});const value=await response.json();el('api-response-status').textContent='GET '+path+' · HTTP '+response.status+' · '+Math.round(performance.now()-start)+' ms';el('api-response').textContent=JSON.stringify(redact(value),null,2);if(response.status===401)el('api-notice').textContent='Sign in on My Keeper to read your account data.';}
 catch(error){el('api-response-status').textContent='Request could not complete';el('api-response').textContent='The service is unavailable or timed out. Check Network Services and try again.';}
 finally{el('api-run').disabled=false;}
});catalog().catch(error=>{el('api-notice').textContent=error.message;el('api-run').disabled=true;});
