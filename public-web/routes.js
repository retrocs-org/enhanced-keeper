'use strict';
// Static assets are real files; pages and HTTP endpoints use the PHP entry point.
function kfxRoute(path) {
 if(typeof path!=='string'||!path.startsWith('/')||path.startsWith('//'))throw Error('Invalid internal route');
 return '/index.php?route='+encodeURIComponent(path);
}
function kfxPage() {
 const route=new URLSearchParams(location.search).get('route');
 const path=route?route.split('?')[0]:location.pathname;
 return {'/network':'/services','/network/servers':'/servers','/network/events':'/events','/network/replays':'/replays','/network/challenges':'/challenges'}[path]||path;
}

let kfxSessionRequest;
function kfxAccountUI(session) {
 if(!session.signed_in){
  for(const id of ['pair-panel','profile-panel','event','upload-replay']){const node=document.getElementById(id);if(node)node.hidden=true;}
  for(const id of ['statistics','history','friends','recovery-output','profile-id']){const node=document.getElementById(id);if(node)node.textContent='';}
 }
 document.querySelectorAll('[data-auth-only]').forEach(node=>node.hidden=!session.signed_in);
 document.querySelectorAll('[data-guest-only]').forEach(node=>node.hidden=session.signed_in);
 document.querySelectorAll('[data-account-label]').forEach(node=>node.textContent=session.signed_in?'My Keeper':'Sign in');
 return session;
}
function kfxSession(force=false) {
 if(force||!kfxSessionRequest) {
  kfxSessionRequest=fetch(kfxRoute('/api/session'),{credentials:'same-origin',signal:AbortSignal.timeout(6000)})
   .then(response=>{if(!response.ok)throw Error('Account service unavailable');return response.json();})
   .then(session=>{if(typeof session.signed_in!=='boolean')throw Error('Invalid account response');return kfxAccountUI(session);})
   .catch(error=>{kfxSessionRequest=undefined;kfxAccountUI({signed_in:false});throw error;});
 }
 return kfxSessionRequest;
}
document.addEventListener('DOMContentLoaded',()=>{kfxSession().catch(()=>{});});
