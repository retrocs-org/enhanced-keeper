'use strict';
async function loadLeaderboard() {
 const status=document.getElementById('leaderboard-state'),body=document.getElementById('leaderboard-players');
 try {
  const response=await fetch(kfxRoute('/api/leaderboard'),{credentials:'same-origin',signal:AbortSignal.timeout(6000)});
  if(!response.ok)throw Error('Leaderboard unavailable');
  const value=await response.json();
  if(!Array.isArray(value.players))throw Error('Invalid leaderboard');
  body.replaceChildren();
  value.players.forEach((player,index)=>{
   const row=document.createElement('tr');
   for(const value of [index+1,player.name,player.wins,player.matches]){const cell=document.createElement('td');cell.textContent=String(value);row.append(cell);}
   const link=document.createElement('a');link.textContent=player.name;link.href=kfxRoute('/keeper/'+player.id);row.children[1].replaceChildren(link);body.append(row);
  });
  document.getElementById('leaderboard-table').hidden=!value.players.length;
  status.textContent=value.players.length?'Ranked by server-verified results.':'No public verified records yet. Completed server-verified matches will appear here.';
 }catch(error){status.textContent='Leaderboard unavailable. Please try again later.';}
}
loadLeaderboard();
