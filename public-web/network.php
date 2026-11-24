<?php
if (!defined('KFX_WEB_PAGE')) { http_response_code(404); exit; }
$isApi=$path==='/api-browser';
?>
<!doctype html><html lang="en"><head><meta charset="utf-8"><script src="/routes.js"></script><meta name="viewport" content="width=device-width,initial-scale=1"><title><?= $isApi?'API browser':'Network services' ?> · Keeper-Enhanced</title><link rel="icon" type="image/svg+xml" href="/assets/keeper-mask.svg"><link rel="stylesheet" href="/site.css"><link rel="stylesheet" href="/style.css"><script src="<?= $isApi?'/api-browser.js':'/portal.js' ?>" defer></script></head><body>
<?php site_header($path); ?>
<main class="wrap portal-main" id="main">
<p class="eyebrow">Keeper-Enhanced network</p><h1><?= $isApi?'Explore the API.':'A place to play together.' ?></h1>
<?php if ($isApi): ?>
<p class="lead">Browse the same service used by the website and the game. Try read requests with your current website session; account changes and host operations are documented below.</p>
<p id="api-notice" role="status" aria-live="polite"></p>
<section class="api-workbench" aria-labelledby="try-title"><div><h2 id="try-title">Try a request</h2><form id="api-explorer"><label for="api-endpoint">Endpoint</label><select id="api-endpoint" name="endpoint"><?php foreach(api_catalog() as $entry) if($entry['interactive']): ?><option value="<?= web_escape($entry['path']) ?>"><?= web_escape($entry['method'].' '.$entry['path']) ?></option><?php endif; ?></select><p id="api-description"></p><button type="submit" id="api-run">Send GET request</button></form><p>Responses stay in this page. Tokens, passwords, recovery codes and CSRF values are redacted. Use <a href="<?= web_escape(web_url('/profile')) ?>">My Keeper</a> to sign in.</p></div><div><p id="api-response-status" role="status">No request sent</p><pre id="api-response" tabindex="0" aria-label="API response">Choose an endpoint and send a request.</pre></div></section>
<section aria-labelledby="catalog-title"><h2 id="catalog-title">Endpoint reference</h2><label for="api-filter">Search endpoints</label><input type="search" id="api-filter" placeholder="Servers, pairing, replays…"><div class="table-wrap"><table id="api-reference"><thead><tr><th>Method / route</th><th>Access</th><th>Purpose</th></tr></thead><tbody><?php foreach(api_catalog() as $entry): ?><tr><td><span class="method"><?= $entry['method'] ?></span><code><?= web_escape($entry['path']) ?></code><small><?= web_escape($entry['group']) ?></small></td><td><?= web_escape($entry['auth']) ?></td><td><?= web_escape($entry['description']) ?></td></tr><?php endforeach; ?></tbody></table></div></section>
<?php else: ?>
<p class="lead">One website for public games, accounts, statistics and game pairing. LAN and direct connections remain available without an account.</p>
<p data-guest-only hidden><a href="<?= web_escape(web_url('/profile')) ?>">Sign in</a> to join events, share replays and connect your game.</p>
<p id="service-summary" role="status" aria-live="polite">Checking service availability…</p>
<section class="service-grid" aria-label="Network services"><article><h2>Server directory</h2><p id="directory-status">Checking directory…</p><a class="button secondary" href="<?= web_escape(web_url('/servers')) ?>">Browse servers</a></article><article><h2>Community accounts</h2><p id="account-status">Checking account service…</p><a class="button secondary" href="<?= web_escape(web_url('/profile')) ?>">Account and statistics</a></article><article><h2>Ways to play</h2><p>Play on LAN, connect directly to a private server, or join a listed public server. Dedicated servers can be private or public, with optional password protection.</p></article><article data-auth-only hidden><h2>Connect your game</h2><p>Start sign-in inside Keeper-Enhanced, then approve its pairing code here.</p><a class="button secondary" href="<?= web_escape(web_url('/device')) ?>">Connect game</a></article></section>
<div class="actions"><button id="service-refresh" type="button">Refresh service status</button></div>
<section><h2>More from the community</h2><div class="actions"><a href="<?= web_escape(web_url('/events')) ?>">Events and tournaments</a><a href="<?= web_escape(web_url('/challenges')) ?>">Cooperative challenges</a><a href="<?= web_escape(web_url('/replays')) ?>">Shared replays</a></div></section>
<?php endif; ?>
</main><?php site_footer(); ?></body></html>
