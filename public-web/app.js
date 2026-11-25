'use strict';
const $=id=>document.getElementById(id);let session={},servers=[],me=null;
function note(text){
 const onAccountPage=!$('login-panel').hidden;
 $('account-status').textContent=onAccountPage?text:'';
 $('notice').textContent=onAccountPage?'':text;
}
function node(tag,text,parent){const n=document.createElement(tag);n.textContent=text;if(parent)parent.append(n);return n;}
async function api(path,data){const r=await fetch(kfxRoute('/api/'+path),{method:data?'POST':'GET',headers:data?{'Content-Type':'application/json','X-CSRF-Token':session.csrf||''}:{},body:data?JSON.stringify(data):undefined,credentials:'same-origin'});const value=await r.json();if(!r.ok)throw Error(value.error||'Request failed');return value;}
function action(label,handler,parent){const b=node('button',label,parent);b.addEventListener('click',()=>Promise.resolve().then(handler).catch(e=>note(e.message)));return b;}
async function refresh(){
 session=await kfxSession(true);const route=kfxPage();
 $('session').textContent=session.signed_in?'Signed in as '+session.name:'Guest · LAN and direct play need no account';
 $('login-panel').hidden=session.signed_in||!['/profile','/device'].includes(route);
 document.body.classList.toggle('account-auth',!$('login-panel').hidden);
 $('pair-panel').hidden=route!=='/device'||!session.signed_in;
 $('profile-panel').hidden=!session.signed_in||route!=='/profile';
 $('event').hidden=!session.signed_in;$('upload-replay').hidden=!session.signed_in;
 if(!session.signed_in){$('recovery-output').textContent='';$('recovery-output').hidden=true;me=null;}
 else if(route==='/profile')await loadProfile();
 else if(['/events','/community'].includes(route))me=await api('me');
 if(['/lair','/servers','/community'].includes(route)){servers=(await api('servers')).servers;drawServers();}
 if(route==='/lair')await Promise.all([loadLairRankings(),loadLairHistory(),loadSiegeTimes()]);
 if(['/events','/community'].includes(route))await loadEvents();
 if(['/community','/challenges','/replays'].includes(route))await loadExtras();
}

async function loadSiegeTimes(){
 const list=$('lair-siege-times');
 try {
  const {times}=await api('siege-times');
  list.replaceChildren();
  for(const row of times)node('li',row.first+' & '+row.second+' · '+Number(row.turns).toLocaleString()+' turns',list);
  $('lair-siege-state').textContent=times.length?'':'No public verified times yet.';
 }catch(error){$('lair-siege-state').textContent=error.message;}
}
async function loadLairHistory(){
 const panel=$('lair-match-list');
 panel.replaceChildren();
 if(!session.signed_in){
  node('a','Sign in to see your matches',panel).href=kfxRoute('/profile');
  return;
 }
 try {
  const {matches}=await api('matches');
  if(!matches.length)node('p','No matches recorded yet.',panel);
  for(const match of matches){
   const card=node('article','',panel);
   const when=new Date(Number(match.started)*1000).toLocaleString();
   node('h3',match.map||'Unknown map',card);
   node('p',when+' · '+(match.your_result||match.status),card);
   for(const keeper of match.keepers||[]){
    node('p',keeper.name+(Number(keeper.together)?' · Party':'')+(keeper.result?' · '+keeper.result:''),card);
   }
   if(match.replay_id)action('Download replay to watch',()=>downloadMatchReplay(match.replay_id),card);
  }
 }catch(error){node('p',error.message,panel);}
}
async function downloadMatchReplay(id){
 const reply=await api('replays/'+id);
 const binary=Uint8Array.from(atob(reply.data),character=>character.charCodeAt(0));
 const url=URL.createObjectURL(new Blob([binary],{type:'application/octet-stream'}));
 const link=document.createElement('a');
 link.href=url;
 link.download='keeper-'+id+'.pck';
 link.click();
 setTimeout(()=>URL.revokeObjectURL(url),1000);
 note('Replay downloaded. Open it in Keeper-Enhanced.');
}
async function loadLairRankings(){
 const body=$('lair-rankings');
 try {
  const {players}=await api('leaderboard');
  body.replaceChildren();
  for(const [index,player] of players.entries()){
   const row=node('tr','',body);
   node('td',String(index+1),row);
   const keeper=node('a',player.name,node('td','',row));
   keeper.href=kfxRoute('/keeper/'+player.id);
   node('td',String(player.wins),row);
   node('td',String(player.matches),row);
  }
  $('lair-rank-table').hidden=players.length===0;
  $('lair-rank-state').textContent=players.length?'':'No verified rankings yet.';
 } catch(error) { $('lair-rank-state').textContent=error.message; }
}
function drawServers(){const q=$('filter').value.toLowerCase(),body=$('servers');body.replaceChildren();for(const s of servers.filter(s=>(s.name+' '+s.map+' '+s.region).toLowerCase().includes(q))){const tr=node('tr','',body);node('td',s.name+(s.password?' 🔒':''),tr);node('td',s.map||'Lobby',tr);node('td',s.players+'/'+s.capacity,tr);node('td',s.engine,tr);node('td',s.kind+' · '+s.region+' · '+s.status,tr);action('Copy address',async()=>{await navigator.clipboard.writeText(s.endpoint);note('Address copied. Use Direct Connect in the game.');},node('td','',tr));}$('server-state').textContent=servers.length?servers.length+' live servers. Refresh to update availability.':'No active servers. LAN and direct connections are still available.';}
async function loadProfile(){
    me=await api('me');
    showProfileSettings();
    const section=$('profile-panel').dataset.section;
    const titles={friends:'Friends',history:'Match history',security:'Account security'};
    $('profile-title').textContent=titles[section]||me.name+'’s Keeper';

    if(section==='profile'){
        $('profile-id').textContent='Account ID: '+me.id;
        if(me.public){
            node('span',' · ', $('profile-id'));
            node('a','View public profile', $('profile-id')).href=kfxRoute('/keeper/'+me.id);
        }
    }
    if(section==='security')$('public').checked=!!me.public;
    if(section==='history'){
        const [profile, history]=await Promise.all([api('profiles/'+me.id),api('matches')]);
        $('statistics').textContent=(profile.verified_stats.matches||0)+' verified matches · '+(profile.verified_stats.wins||0)+' verified wins'+(profile.achievements.length?' · '+profile.achievements.join(', '):'');
        $('history').replaceChildren();
        if(!history.matches.length)node('p','No matches recorded yet.', $('history'));
        for(const match of history.matches){
            const article=node('article','', $('history'));
            node('strong',match.map||'Match',article);
            node('p',new Date(match.started*1000).toLocaleString()+' · '+match.status+' · '+match.verification+' · Match ID: '+match.id,article);
        }
    }
    if(section==='friends'){
        $('friend-account-id').textContent='Your account ID: '+me.id;
        const {friends}=await api('friends');
        $('friends').replaceChildren();
        if(!friends.length)node('li','No friends or pending requests yet.', $('friends'));
        for(const friend of friends){
            const row=node('li',friend.name+' · '+friend.status+' ', $('friends'));
            if(friend.status==='pending'&&friend.requester!==me.id){
                action('Accept',async()=>{await api('friends',{user:friend.id});await loadProfile();},row);
            }
            action('Remove',async()=>{await api('friends',{user:friend.id,action:'remove'});await loadProfile();},row);
        }
    }
}
async function loadEvents(){$('events').replaceChildren();const events=(await api('events')).events;for(const e of events){const a=node('article','', $('events'));node('h3',e.title,a);node('p',new Date(e.starts*1000).toLocaleString()+' · '+e.kind+' · '+e.entrants+' signed up',a);node('p',e.description,a);if(session.signed_in)action('Join event',async()=>{await api('events/'+e.id+'/join',{});await loadEvents();},a);if(e.kind==='tournament'){action('Show bracket',async()=>{const b=await api('events/'+e.id+'/bracket');if(!b.games){node('p',b.round_one.map(p=>p.a.name+' vs '+(p.b?p.b.name:'bye')).join(' • ')||'Awaiting entrants',a);return;}for(const g of b.games){const row=node('article','Round '+g.round+' · '+g.a_name+' vs '+g.b_name+' · '+g.status+(g.winner?' · Winner: '+g.winner_name:''),a);if(me&&me.id===e.owner&&g.status==='pending'){const label=node('label','Verified match ID',row),input=document.createElement('input');input.maxLength=32;label.append(input);action('Record verified result',async()=>{await api('events/'+e.id+'/bracket/report',{round:g.round,position:g.position,match:input.value.trim()});note('Bracket advanced.');await loadEvents();},row);}}},a);if(me&&me.id===e.owner)action('Start tournament',async()=>{await api('events/'+e.id+'/bracket/start',{});note('Bracket created. Registration is closed.');await loadEvents();},a);}}if(!events.length)node('p','No scheduled events yet.', $('events'));}
async function loadExtras(){if(['/community','/challenges'].includes(kfxPage())){$('challenges').replaceChildren();const c=await api('challenges');for(const v of c.challenges){const a=node('article','', $('challenges'));node('h3',v.title,a);node('p',v.map+' · '+v.description,a);if(session.signed_in){const label=node('label','Verified winning match ID',a),input=document.createElement('input');input.maxLength=32;label.append(input);action('Record completion',async()=>{await api('challenges/'+v.id+'/complete',{match:input.value.trim()});note('Challenge completed.');await loadExtras();},a);}}if(!c.challenges.length)node('p','Community challenges will appear here when the service administrator adds them.', $('challenges'));}if(['/community','/replays'].includes(kfxPage())){$('replays').replaceChildren();const r=await api('replays');for(const v of r.replays){const a=node('article','Replay '+v.id.slice(0,8)+' · '+Math.round(v.size/1024)+' KiB ', $('replays'));action('Download',async()=>{const x=await api('replays/'+v.id);const bytes=Uint8Array.from(atob(x.data),c=>c.charCodeAt(0));const actual=Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',bytes)),b=>b.toString(16).padStart(2,'0')).join('');if(actual!==x.sha256)throw Error('Replay checksum differs');const url=URL.createObjectURL(new Blob([bytes],{type:'application/octet-stream'}));const link=node('a','');link.href=url;link.download='keeperfx-'+v.id+'.pck';link.click();setTimeout(()=>URL.revokeObjectURL(url),1000);note('Replay downloaded. Required engine: '+x.engine);},a);}if(!r.replays.length)node('p','No shared replays yet.', $('replays'));}}
for(const id of ['login','pair','friend','event'])$(id).addEventListener('submit',async e=>{e.preventDefault();const d=Object.fromEntries(new FormData(e.target));try{if(id==='login')await api('login',d);if(id==='pair')await api('device/approve',{user_code:d.code.trim().toUpperCase()});if(id==='friend')await api('friends',d);if(id==='event')await api('events',{...d,starts:Math.floor(new Date(d.starts).getTime()/1000)});note(id==='pair'?'Game approved. Return to KeeperFX.':'Saved.');await refresh();}catch(e){note(e.message);}});
$('refresh').onclick=()=>refresh().catch(e=>note(e.message));$('filter').oninput=drawServers;$('logout').onclick=async()=>{try{await api('logout',{});await refresh();}catch(e){note(e.message);}};$('public').onchange=async()=>{try{await api('profile',{public:$('public').checked});note('Profile privacy saved.');}catch(e){note(e.message);}};$('upload-replay').addEventListener('submit',async e=>{e.preventDefault();try{const file=e.target.elements.replay.files[0];if(!file||file.size>2*1024*1024)throw Error('Choose a replay up to 2 MiB.');const data=await new Promise((resolve,reject)=>{const reader=new FileReader();reader.onload=()=>resolve(String(reader.result).split(',')[1]);reader.onerror=()=>reject(Error('Could not read the file.'));reader.readAsDataURL(file);});await api('replays',{match:e.target.elements.match.value.trim(),data});note('Replay shared.');await loadExtras();}catch(e){note(e.message);}});
refresh().then(async()=>{if(kfxPage().startsWith('/keeper/')){const profile=await api('profiles/'+kfxPage().split('/').pop());const panel=$('public-profile');panel.hidden=false;node('h2',profile.name+'’s Keeper',panel);node('p',(profile.verified_stats.matches||0)+' verified matches · '+(profile.verified_stats.wins||0)+' wins',panel);node('p',profile.achievements.join(' · ')||'No achievements yet.',panel);}}).catch(e=>note(e.message));

$('recovery-codes-form').addEventListener('submit',async event=>{event.preventDefault();try{const result=await api('recovery/codes',Object.fromEntries(new FormData(event.target)));event.target.reset();$('recovery-output').textContent=result.codes.join('\n');$('recovery-output').hidden=false;note(result.message);}catch(error){note(error.message);}});
$('recover').addEventListener('submit',async event=>{event.preventDefault();try{const result=await api('recovery/reset',Object.fromEntries(new FormData(event.target)));event.target.reset();showAccountMode('login');note(result.message);await refresh();}catch(error){note(error.message);}});

const route=kfxPage();
for(const [id,paths] of Object.entries({'server-panel':['/lair','/community','/servers'],'events-panel':['/community','/events'],'challenge-panel':['/community','/challenges'],'replay-panel':['/community','/replays']}))$(id).hidden=!paths.includes(route);

let cloudTicket = '';
const accountModes = {
    activation: ['cloud-resend', 'Resend activation email', 'Enter the username, email address, and password you used to register.'],
    login: ['login', 'Sign in', 'Access your cloud profile, friends, and match history.'],
    signup: ['cloud-signup', 'Create your account', 'Keep your cloud profile and match history together. We’ll email you a six-digit code to verify your account.'],
    verify: ['cloud-verify', 'Check your email', 'Enter the six-digit verification code. It expires after 15 minutes.'],
    recover: ['recover', 'Recover your account', 'Use an offline recovery code to set a new password. This signs you out of other sessions.'],
};
function showAccountMode(mode, moveFocus = true) {
    const [form, title, intro] = accountModes[mode];
    for (const [formId] of Object.values(accountModes)) $(formId).hidden = formId !== form;
    $('account-heading').textContent = title;
    $('account-intro').textContent = intro;
    $('account-status').textContent = '';
    if (moveFocus) $('account-heading').focus();
}
for (const [id,route] of [['cloud-signup','cloud/signup'],['cloud-resend','cloud/resend']]) {
    $(id).addEventListener('submit', async event => {
        event.preventDefault();
        try {
            const result = await api(route, Object.fromEntries(new FormData(event.target)));
            cloudTicket = result.ticket;
            event.target.reset();
            showAccountMode('verify', false);
            $('cloud-verify').elements.code.focus();
            note(id==='cloud-resend'?'If a pending account matches, we sent a new code.':'Enter the six-digit code sent to your email.');
        } catch (error) { note(error.message); }
    });
}
$('cloud-verify').addEventListener('submit', async event => {
    event.preventDefault();
    try {
        await api('cloud/verify', {ticket: cloudTicket, code: event.target.elements.code.value, browser: true});
        cloudTicket = '';
        event.target.reset();
        showAccountMode('login', false);
        await refresh();
        note('Email verified.');
    } catch (error) { note(error.message); }
});

let emailTicket='';
const avatarInputs=[...document.querySelectorAll('.avatar-picker input')];
for(const tile of document.querySelectorAll('[data-avatar]'))kfxAvatar(tile,tile.dataset.avatar);
function showProfileSettings(){
    const portrait=$('profile-avatar');
    kfxAvatar(portrait,me.avatar);
    portrait.hidden=false;
    $('profile-email').textContent=me.email?'Current email: '+me.email:'No email is linked to this account.';
    for(const input of avatarInputs)input.checked=Number(input.value)===Number(me.avatar);
}
for(const input of avatarInputs)input.addEventListener('change',async()=>{
    if(!input.checked)return;
    for(const option of avatarInputs)option.disabled=true;
    try {
        const result=await api('profile/avatar',{avatar:Number(input.value)});
        me.avatar=result.avatar;
        session.avatar=result.avatar;
        kfxAccountUI(session);
        note('Avatar saved.');
    } catch(error) { note(error.message); }
    finally {
        if(me)showProfileSettings();
        for(const option of avatarInputs)option.disabled=false;
    }
});
function profileSubmit(id,submit){
    $(id).addEventListener('submit',async event=>{
        event.preventDefault();
        const form=event.target, button=event.submitter;
        if(button)button.disabled=true;
        try { await submit(Object.fromEntries(new FormData(form)));form.reset(); }
        catch(error) { note(error.message); }
        finally { if(button)button.disabled=false; }
    });
}
profileSubmit('profile-email-form',async data=>{
    emailTicket=(await api('profile/email',data)).ticket;
    $('profile-email-verify').hidden=false;
    $('profile-email-verify').elements.code.focus();
    note('Enter the code sent to your new email address.');
});
profileSubmit('profile-email-verify',async data=>{
    const result=await api('profile/email/verify',{ticket:emailTicket,code:data.code});
    emailTicket='';
    me.email=result.email;
    $('profile-email-verify').hidden=true;
    showProfileSettings();
    note('Email address updated.');
});
profileSubmit('profile-password-form',async data=>{
    await api('profile/password',data);
    emailTicket='';
    $('profile-email-verify').hidden=true;
    note('Password changed. Other sessions and recovery codes were revoked.');
});
profileSubmit('profile-delete-form',async data=>{
    await api('profile/delete',data);
    emailTicket='';
    $('profile-email-verify').hidden=true;
    await refresh();
    note('Your cloud account was deleted.');
});
