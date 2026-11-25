<?php
if (!defined('KFX_WEB_PAGE')) { http_response_code(404); exit; }
$pageTitles=['/servers'=>['Find your next game.','Browse public and dedicated servers. Use the same directory inside Keeper-Enhanced.'],'/events'=>['Meet in the dungeon.','Friendly matches, tournaments and community events.'],'/profile'=>['Your Keeper account.','Your profile, friends and verified match history, shared with the game.'],'/device'=>['Connect your game.','Approve only the pairing code displayed by your own Keeper-Enhanced game.'],'/replays'=>['Watch it unfold.','Share and download completed-match replays.'],'/challenges'=>['Conquer together.','Cooperative challenges and verified completions.']];
$accountModes=[
    'login'=>['Sign in','Access your cloud profile, friends, and match history.'],
    'register'=>['Create your account','Keep your cloud profile and match history together. We’ll email you a six-digit code to verify your account.'],
    'activation'=>['Resend activation email','Enter the username, email address, and password you used to register.'],
    'recover'=>['Reset your password','Use an offline recovery code to set a new password. This signs you out of other sessions.'],
];
$requestedMode=$_GET['mode']??'login';
$accountMode=is_string($requestedMode)&&isset($accountModes[$requestedMode])?$requestedMode:'login';
if($path==='/lair')$pageTitles['/lair']=['The Lair','Find a public game or see who leads the verified rankings.'];
[$heading,$description]=$pageTitles[$path]??['The community dungeon.','Find games, meet other keepers and follow the community.'];
?>
<!doctype html><html lang="en"><head><meta charset="utf-8"><script src="/routes.js"></script><meta name="viewport" content="width=device-width,initial-scale=1"><title>Keeper-Enhanced • Community</title><link rel="icon" type="image/svg+xml" href="/assets/keeper-mask.svg"><link rel="stylesheet" href="/site.css"><link rel="stylesheet" href="/style.css"><script src="/app.js" defer></script></head><body><?php site_header($path); ?><main class="wrap portal-main" id="main"><div class="community-intro"><p class="eyebrow">Keeper-Enhanced community</p><h1><?= web_escape($heading) ?></h1><p class="lead"><?= web_escape($description) ?></p></div><noscript><p>Enable JavaScript to use account and community controls. The server directory and API remain available through the documented endpoints.</p></noscript><p id="session">Guest · LAN and direct play need no account</p><p id="notice" role="status" aria-live="polite"></p><section id="public-profile" hidden></section><section id="login-panel" class="account-panel" hidden>
    <p class="account-eyebrow">Keeper Enhanced account</p>
    <h1 id="account-heading" tabindex="-1"><?= web_escape($accountModes[$accountMode][0]) ?></h1>
    <p id="account-intro"><?= web_escape($accountModes[$accountMode][1]) ?></p>
    <div id="account-status" role="status" aria-live="polite"></div>
    <form method="post" id="login"<?= $accountMode==='login'?'':' hidden' ?>>
        <label>Username<input name="name" required minlength="3" maxlength="24" autocomplete="username"></label>
        <label>Password<input name="password" type="password" required minlength="12" maxlength="128" autocomplete="current-password"></label>
        <button>Sign in</button>
    </form>
    <form method="post" id="cloud-signup"<?= $accountMode==='register'?'':' hidden' ?>>
        <label>Username<input name="name" required minlength="3" maxlength="24" pattern="[A-Za-z0-9_\x2D]{3,24}" autocomplete="username" aria-describedby="signup-name-hint"><small id="signup-name-hint">3–24 letters, numbers, underscores, or hyphens.</small></label>
        <label>Email<input name="email" type="email" required maxlength="254" autocomplete="email"></label>
        <label>Password<input name="password" type="password" required minlength="12" maxlength="128" autocomplete="new-password" aria-describedby="signup-password-hint"><small id="signup-password-hint">At least 12 characters.</small></label>
        <button>Send verification code</button>
    </form>
    <form method="post" id="cloud-verify" hidden>
        <label>Email verification code<input name="code" required pattern="[0-9]{6}" maxlength="6" inputmode="numeric" autocomplete="one-time-code"></label>
        <button>Verify email</button>
    </form>
    <form method="post" id="cloud-resend"<?= $accountMode==='activation'?'':' hidden' ?>>
        <label>Username<input name="name" required minlength="3" maxlength="24" autocomplete="username"></label>
        <label>Email<input name="email" type="email" required maxlength="254" autocomplete="email"></label>
        <label>Password<input name="password" type="password" required minlength="12" maxlength="128" autocomplete="current-password"></label>
        <button>Resend activation email</button>
    </form>
    <form method="post" id="recover"<?= $accountMode==='recover'?'':' hidden' ?>>
        <label>Username<input name="name" required autocomplete="username"></label>
        <label>Recovery code<input name="code" required autocomplete="off"></label>
        <label>New password<input name="password" type="password" minlength="12" maxlength="128" required autocomplete="new-password"></label>
        <button>Reset password</button>
    </form>
    <nav class="account-nav" aria-label="Account options">
        <?php foreach(['register'=>'Create an account','login'=>'Sign in','activation'=>'Resend activation email','recover'=>'Forgot password?'] as $mode=>$label): ?>
            <a href="<?= web_escape(web_url($path==='/device'?'/device':'/profile').($mode==='login'?'':'&mode='.$mode)) ?>"<?= $mode===$accountMode?' aria-current="page"':'' ?>><?= web_escape($label) ?></a>
        <?php endforeach; ?>
    </nav>
</section><section id="pair-panel" hidden><h2>Connect the game</h2><p>Choose Sign In inside the game, then enter its pairing code here. Approve only a code displayed by your own game.</p><form method="post" id="pair"><label>Game pairing code<input name="code" required minlength="8" maxlength="8" autocomplete="off"></label><button>Approve this game</button></form></section><section id="server-panel"<?= in_array($path,['/lair','/servers','/community'],true)?'':' hidden' ?>><div class="section-title"><h2>Server browser</h2><button id="refresh">Refresh</button></div><label>Filter servers<input id="filter" type="search" placeholder="Name, map or region"></label><div class="table-wrap"><table><thead><tr><th>Server</th><th>Map</th><th>Players</th><th>Engine</th><th>Connection</th><th></th></tr></thead><tbody id="servers"></tbody></table></div><p id="server-state">Loading directory…</p></section><?php if($path==='/lair'): ?>
<section id="rankings">
    <h2>Verified rankings</h2>
    <p id="lair-rank-state" role="status">Loading verified records…</p>
    <div class="table-wrap" id="lair-rank-table" hidden>
        <table><thead><tr><th scope="col">Rank</th><th scope="col">Keeper</th><th scope="col">Verified wins</th><th scope="col">Verified matches</th></tr></thead><tbody id="lair-rankings"></tbody></table>
    </div>
</section>
<?php endif; ?>
<section id="profile-panel" hidden><div class="section-title"><span id="profile-avatar" class="profile-avatar" role="img" aria-label="Keeper avatar" hidden></span><h2 id="profile-title">My Keeper</h2><button id="logout">Sign out</button></div><p id="profile-id"></p><label><input id="public" type="checkbox"> Make my profile and verified statistics public</label><div id="statistics"></div><h3>Account recovery</h3><p>Generate recovery codes and store them offline. New codes replace all previous codes.</p><form method="post" id="recovery-codes-form"><label>Current password<input name="password" type="password" required autocomplete="current-password"></label><button id="recovery-codes">Generate recovery codes</button></form><pre id="recovery-output" hidden></pre><h3>Friends</h3><form method="post" id="friend"><label>Friend’s account ID<input name="user" required maxlength="32"></label><button>Send request</button></form><ul id="friends"></ul><h3>Match history</h3><p>Casual and player-confirmed results are separate from server-verified results. Disconnects do not count as verified wins.</p><div id="history"></div></section><section id="events-panel"<?= in_array($path,['/events','/community'],true)?'':' hidden' ?>><h2>Community calendar</h2><div id="events"></div><form method="post" id="event" hidden><label>Event title<input name="title" required maxlength="80"></label><label>Format<select name="kind"><option value="match">Friendly match</option><option value="tournament">Tournament</option><option value="cooperative">Cooperative challenge</option></select></label><label>When<input name="starts" type="datetime-local" required></label><label>Description<textarea name="description" maxlength="1000"></textarea></label><button>Schedule event</button></form></section><section id="challenge-panel"<?= in_array($path,['/challenges','/community'],true)?'':' hidden' ?>><h2>Cooperative challenges</h2><div id="challenges"></div></section><section id="replay-panel"<?= in_array($path,['/replays','/community'],true)?'':' hidden' ?>><h2>Replays</h2><p>Downloads require a matching engine version. Replay sharing is separate from live spectating.</p><div id="replays"></div><form method="post" id="upload-replay" hidden><label>Completed match ID<input name="match" required maxlength="32"></label><label>Replay file (up to 2 MiB)<input name="replay" type="file" accept=".pck" required></label><button>Share replay</button></form></section></main><?php site_footer(); ?></body></html>
