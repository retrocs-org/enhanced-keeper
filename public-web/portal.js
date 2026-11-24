'use strict';
async function refreshServices(){
 const set=(id,text)=>{const el=document.getElementById(id);if(el)el.textContent=text;};
 set('service-summary','Checking service availability…');
 const read=async path=>{const response=await fetch(kfxRoute(path),{credentials:'same-origin',signal:AbortSignal.timeout(6000)});if(!response.ok)throw Error('Service unavailable');return response.json();};
 const [health,directory,session]=await Promise.allSettled([read('/api/health'),read('/api/servers'),kfxSession()]);
 set('service-summary',health.status==='fulfilled'?'Community network services are available.':'Service health check unavailable. Account and directory availability is checked separately.');
 set('directory-status',directory.status==='fulfilled'?directory.value.servers.length+' active server'+(directory.value.servers.length===1?'':'s')+'.':'Directory unavailable. Try again shortly.');
 set('account-status',session.status==='fulfilled'?(session.value.signed_in?'Signed in as '+session.value.name+'. Your account is shared with the game.':'Account service available. Sign in or continue as a guest.'):'Account service unavailable. Existing LAN/direct games are independent.');
}
document.getElementById('service-refresh')?.addEventListener('click',()=>refreshServices());refreshServices();
