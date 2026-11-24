// Exercise guest/signed-in navigation and controls without a browser dependency.
const vm=require('node:vm'),fs=require('node:fs'),assert=require('node:assert/strict');
class Element {
 constructor(tag='div'){this.tag=tag;this.children=[];this.hidden=false;this.value='';this.textContent='';}
 append(child){this.children.push(child);}
 replaceChildren(...children){this.children=children;}
 addEventListener(){}
 scrollIntoView(){}
}
async function check(path,signedIn,failSession=false) {
 const nodes=new Map(),get=id=>{if(!nodes.has(id))nodes.set(id,new Element());return nodes.get(id);};
 for(const id of ['login-panel','pair-panel','profile-panel','event','upload-replay'])get(id).hidden=true;
 const privateLink=new Element('a');privateLink.hidden=true;
 const guestNotice=new Element();guestNotice.hidden=true;
 const accountLink=new Element('a');accountLink.textContent='Sign in';
 const calls=[];let active=signedIn;
 const context=vm.createContext({
  document:{getElementById:get,createElement:tag=>new Element(tag),addEventListener(){},querySelectorAll:selector=>({'[data-auth-only]':[privateLink],'[data-guest-only]':[guestNotice],'[data-account-label]':[accountLink]}[selector]||[])},
  location:{search:'?route='+encodeURIComponent(path),pathname:'/index.php'},URLSearchParams,AbortSignal,
  fetch:async(url)=>{
   const route=new URLSearchParams(url.split('?')[1]).get('route');calls.push(route);
   if(route==='/api/session')return {ok:!failSession,json:async()=>({signed_in:active,name:active?'Keeper':'',csrf:'fixture'})};
   const value={
    '/api/me':{id:'keeper',name:'Keeper'},
    '/api/events':{events:[{id:'event',title:'Public match',starts:1,kind:'match',entrants:0,description:'Public event',owner:'another'}]},
    '/api/replays':{replays:[]},'/api/challenges':{challenges:[]}
   }[route];
   assert.ok(value,'Unexpected API request: '+route);
   return {ok:true,json:async()=>value};
  }
 });
 for(const file of ['routes.js','app.js'])vm.runInContext(fs.readFileSync('public-web/'+file,'utf8'),context);
 assert.equal(get('event').hidden,true,'account forms must start hidden');
 await new Promise(resolve=>setImmediate(resolve));
 const text=node=>[node.textContent,...node.children.map(text)].join(' ');
 if(failSession){assert.equal(privateLink.hidden,true);assert.equal(get('event').hidden,true);assert.deepEqual(calls,['/api/session']);return;}
 assert.equal(privateLink.hidden,!signedIn);assert.equal(guestNotice.hidden,signedIn);
 assert.equal(accountLink.textContent,signedIn?'My Keeper':'Sign in');
 assert.equal(get('event').hidden,!signedIn);assert.equal(get('upload-replay').hidden,!signedIn);
 if(path==='/network/events')assert.equal(text(get('events')).includes('Join event'),signedIn);
 if(path==='/network/replays'){assert.ok(calls.includes('/api/replays'));assert.ok(!calls.includes('/api/challenges'));}
 if(!signedIn)assert.ok(!calls.some(route=>['/api/me','/api/friends','/api/matches'].includes(route)));
 if(signedIn){active=false;await vm.runInContext('refresh()',context);assert.equal(privateLink.hidden,true);assert.equal(get('event').hidden,true);assert.ok(!text(get('events')).includes('Join event'));}
}
(async()=>{
 await check('/network/events',false);
 await check('/network/events',true);
 await check('/network/replays',false);
 await check('/network/events',false,true);
 console.log('PASS: Network subpages keep public listings, hide guest account actions, handle logout and fail closed on session errors.');
})().catch(error=>{console.error(error);process.exitCode=1;});
