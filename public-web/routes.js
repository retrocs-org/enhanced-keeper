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
function kfxAvatar(node, avatar) {
 const index=Math.max(0,Math.min(14,(Number(avatar)||1)-1));
 node.style.backgroundPosition=(index%5)*25+'% '+Math.floor(index/5)*50+'%';
}
function kfxAccountUI(session) {
 if(!session.signed_in){
  for(const id of ['pair-panel','profile-panel','event','upload-replay']){const node=document.getElementById(id);if(node)node.hidden=true;}
  for(const id of ['statistics','history','friends','recovery-output','profile-id']){const node=document.getElementById(id);if(node)node.textContent='';}
 }
 document.querySelectorAll('[data-auth-only]').forEach(node=>node.hidden=!session.signed_in);
 document.querySelectorAll('[data-guest-only]').forEach(node=>node.hidden=session.signed_in);
 const menu=document.querySelector('.account-menu');
 if(menu){
  if(session.signed_in)kfxAvatar(menu.querySelector('[data-account-avatar]'),session.avatar);
  else menu.open=false;
 }
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
document.addEventListener('DOMContentLoaded',()=>{
 const menu=document.querySelector('.account-menu');
 if(menu){
  document.addEventListener('click',event=>{
   if(menu.open&&!menu.contains(event.target))menu.open=false;
  });
  menu.addEventListener('keydown',event=>{
   if(event.key==='Escape'&&menu.open){
    event.preventDefault();
    menu.open=false;
    menu.querySelector('summary').focus();
   }
  });
 }
 kfxSession().catch(()=>{});
});
