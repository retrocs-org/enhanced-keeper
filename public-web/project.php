<?php
if (!defined('KFX_WEB_PAGE')) { http_response_code(404); exit; }
$titles=['/features'=>'Features','/leaderboard'=>'Leaderboard','/downloads'=>'Downloads'];
?>
<!doctype html><html lang="en"><head><meta charset="utf-8"><script src="/routes.js"></script><meta name="viewport" content="width=device-width,initial-scale=1"><title><?= web_escape($titles[$path]) ?> · Keeper-Enhanced</title><link rel="icon" type="image/svg+xml" href="/assets/keeper-mask.svg"><link rel="stylesheet" href="/site.css"><link rel="stylesheet" href="/style.css"><?php if($path==='/leaderboard'): ?><script src="/community-pages.js" defer></script><?php endif; ?></head><body>
<?php site_header($path); ?>
<?php if($path==='/features'): ?>
<main id="main"><?php $standaloneFeatures=true;require __DIR__.'/feature-section.php'; ?></main>
<?php else: ?>
<main id="main" class="wrap portal-main">
<?php if($path==='/leaderboard'): ?>
<p class="eyebrow">Keeper records</p><h1>Names from the depths.</h1><p class="lead">Public Keepers ranked by verified wins, then verified matches played. Casual results and private profiles are excluded.</p>
<p id="leaderboard-state" role="status" aria-live="polite">Loading verified records…</p>
<div class="table-wrap" id="leaderboard-table" hidden><table><thead><tr><th>Rank</th><th>Keeper</th><th>Verified wins</th><th>Verified matches</th></tr></thead><tbody id="leaderboard-players"></tbody></table></div>
<p class="feature-details" data-guest-only hidden>Want to appear here? <a href="<?= web_escape(web_url('/profile')) ?>">Sign in</a>, make your profile public and complete a server-verified match.</p>
<p class="feature-details" data-auth-only hidden>Make your profile public in <a href="<?= web_escape(web_url('/profile')) ?>">My Keeper</a> to share your verified results.</p>
<?php else: $download=current_release(); ?>
<p class="eyebrow">Enter the dungeon</p><h1>Get Keeper-Enhanced.</h1><p class="lead">A native macOS app for Apple Silicon and Intel. Bring your own Dungeon Keeper game files and import them with the included tools.</p>
<section><h2>macOS Universal</h2><p>Requires macOS 13 or later. One build supports both Apple Silicon and Intel Macs.</p>
<?php if($download): ?><p>Verified release: <strong><?= web_escape($download['version']) ?></strong></p><div class="actions"><?php foreach($download['assets'] as $asset): ?><a class="button <?= $asset['type']==='dmg'?'primary':'secondary' ?>" href="<?= web_escape($asset['url']) ?>">Download <?= strtoupper(web_escape($asset['type'])) ?></a><?php endforeach; ?></div><details><summary>SHA-256 checksums</summary><?php foreach($download['assets'] as $asset): ?><p><?= strtoupper(web_escape($asset['type'])) ?>: <code><?= web_escape($asset['sha256']) ?></code></p><?php endforeach; ?></details>
<?php else: ?><p>Available release packages and their notes are listed on GitHub.</p><div class="actions"><a class="button primary" href="https://github.com/retrocs-org/KeeperFX-E/releases">Browse GitHub releases</a></div><?php endif; ?></section>
<section><h2>Bring your own game files.</h2><p>Original graphics, sounds and game data are not bundled. Use your own Dungeon Keeper installation with the import tools, then choose display and controller settings inside the game.</p><a href="<?= web_escape(web_url('/features')) ?>">Explore Keeper-Enhanced features</a></section>
<?php endif; ?>
</main><?php endif; ?>
<?php site_footer(); ?></body></html>
