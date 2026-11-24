<?php
if (!defined('KFX_WEB_PAGE')) { http_response_code(404); exit; }
$release=current_release();
function escape(string $value): string { return htmlspecialchars($value,ENT_QUOTES,'UTF-8'); }
?>
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="description" content="KeeperFX-E brings classic dungeon strategy to macOS, with universal builds, controller support and integrated settings.">
<title>KeeperFX-E — Build your dungeon</title>
<link rel="stylesheet" href="/site.css"><link rel="stylesheet" href="/style.css">
</head>
<body>
<?php site_header('/'); ?>
<main id="main">
<section class="hero wrap">
  <p class="eyebrow"><span class="status-dot" aria-hidden="true"></span>Classic strategy. Made at home on your Mac.</p>
  <h1>Your dungeon.<br><span>Your rules.</span></h1>
  <p class="hero-copy">Return underground with KeeperFX-E. A native macOS take on KeeperFX, with modern controls, a familiar dungeon and more ways to play together.</p>
  <div class="actions"><a class="button primary" href="https://github.com/retrocs-org/KeeperFX-E/releases">Browse downloads </a><a class="button secondary" href="/services">Explore the network </a></div>
  <?php if ($release): ?>
  <section class="release-downloads" aria-label="Latest release"><h2>Latest release: <?= escape($release['version']) ?></h2>
  <?php foreach ($release['assets'] as $asset): ?><p><a href="<?= escape($asset['url']) ?>">Download <?= strtoupper(escape($asset['type'])) ?></a><br><small>SHA-256: <code><?= escape($asset['sha256']) ?></code></small></p><?php endforeach; ?>
  </section><?php endif; ?>
  <p class="compatibility">macOS 13+ <span>·</span> Apple Silicon &amp; Intel <span>·</span> Open source</p>
  <div class="hero-note"><span class="note-index">01 / THE PROJECT</span><p>The dungeon you remember, with room to grow.<br>Built for mouse, keyboard and controller.</p><span class="note-glyph" aria-hidden="true">↘</span></div>
</section>
<section class="features wrap" id="features" aria-labelledby="features-title">
  <div class="section-heading"><p class="eyebrow">Small details. Better play.</p><h2 id="features-title">Settle in.<br>Take control.</h2></div>
  <div class="feature-grid">
    <article><span class="feature-index">01</span><h3>Native on your Mac</h3><p>A universal macOS build for Apple Silicon and Intel, with display and fullscreen settings inside the game.</p></article>
    <article><span class="feature-index">02</span><h3>Play your way</h3><p>Xbox and DualSense support, remappable buttons, camera controls and illustrated controller guides.</p></article>
    <article><span class="feature-index">03</span><h3>Keep your progress</h3><p>Integrated save management, rotating backups and settings that stay with your player profile.</p></article>
    <article><span class="feature-index">04</span><h3>Meet underground</h3><p>LAN and direct connections, plus optional community accounts, server listings and match history.</p></article>
  </div>
</section>
<section class="getting-started wrap" aria-labelledby="start-title">
  <div><p class="eyebrow">Bring your own dungeon</p><h2 id="start-title">A few steps<br>to the underworld.</h2><p>You'll need your own Dungeon Keeper game files. The import tools help bring your graphics, sound and data into the game.</p></div>
  <ol class="steps"><li><span>01</span><div><h3>Get KeeperFX-E</h3><p>Find available builds on the <a href="https://github.com/retrocs-org/KeeperFX-E/releases">release page</a>, or build from source.</p></div></li><li><span>02</span><div><h3>Import your game files</h3><p>Use your own copy of Dungeon Keeper and the included import tools. Original game assets are not included.</p></div></li><li><span>03</span><div><h3>Make yourself at home</h3><p>Choose your controls and display settings, then start a dungeon or find a game.</p></div></li></ol>
</section>
<section class="getting-started wrap" aria-labelledby="network-title"><div><p class="eyebrow">One connected dungeon</p><h2 id="network-title">Find a game.<br>Make it yours.</h2><p>Browse live servers, connect your game account and follow your verified statistics on this website.</p></div><div class="repository-links"><a href="/servers">Server browser</a><a href="/profile">Account and statistics</a><a href="/services">Network services</a><a href="/api-browser">API browser</a></div></section>
<section class="project-links wrap" aria-labelledby="project-title"><div><p class="eyebrow">Built in the open</p><h2 id="project-title">Follow along.<br>Make it better.</h2><p>Read the code, report a problem or help shape the next update.</p></div><div class="repository-links"><a href="https://github.com/retrocs-org/KeeperFX-E"><span>GitHub<small>Code, issues &amp; releases</small></span></a><a href="https://gitlab.kladhest.se/retrocs-org/keeperfx-e"><span>GitLab<small>Source mirror &amp; releases</small></span></a></div></section>
</main>
<?php site_footer(); ?>
</body>
</html>
