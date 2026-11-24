<?php
if (!defined('KFX_WEB_PAGE')) { http_response_code(404); exit; }
$release=current_release();
function escape(string $value): string { return htmlspecialchars($value,ENT_QUOTES,'UTF-8'); }
?>
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="description" content="Build your dungeon with KeeperFX-E. Native macOS, Xbox and DualSense controls, and a connected community of keepers.">
<title>KeeperFX-E — Your dungeon awaits</title>
<link rel="icon" type="image/svg+xml" href="/assets/keeper-sigil.svg">
<link rel="stylesheet" href="/site.css"><link rel="stylesheet" href="/style.css">
</head>
<body>
<?php site_header('/'); ?>
<main id="main">
<section class="hero wrap" aria-labelledby="hero-title">
  <div class="hero-content"><p class="eyebrow">Evil has a new home on macOS</p>
  <h1 id="hero-title">Your dungeon.<br><span>Your dominion.</span></h1>
  <p class="hero-copy">Carve out your kingdom beneath the surface. KeeperFX-E is a fork of <a href="https://github.com/dkfans/keeperfx">KeeperFX</a>, bringing classic dungeon strategy to your Mac with modern controls and more ways to play together.</p>
  <div class="actions"><a class="button primary" href="https://github.com/retrocs-org/KeeperFX-E/releases">Browse downloads</a><a class="button secondary" href="/servers">Find a game</a></div>
  <p class="compatibility">macOS 13+ <span>·</span> Apple Silicon &amp; Intel</p>
  <p class="ownership-note">Bring your own Dungeon Keeper game files.<br>The included import tools take care of the rest.</p></div>
  <div class="hero-art"><img src="/assets/dungeon-heart.svg" width="1000" height="850" alt="An isometric dungeon with firelit chambers, lava channels and a horned red dungeon heart."></div>
</section>
<div class="realm-strip wrap" aria-label="Ways to play"><span>Build your realm</span><span>Command your creatures</span><span>Challenge other keepers</span></div>
<?php if ($release): ?>
<section class="release-downloads wrap" aria-label="Latest release"><p class="eyebrow">Fresh from the forge</p><h2>Latest release: <?= escape($release['version']) ?></h2>
<?php foreach ($release['assets'] as $asset): ?><p><a href="<?= escape($asset['url']) ?>">Download <?= strtoupper(escape($asset['type'])) ?></a><small>SHA-256: <code><?= escape($asset['sha256']) ?></code></small></p><?php endforeach; ?></section>
<?php endif; ?>
<section class="features wrap" id="features" aria-labelledby="features-title">
  <div class="section-heading"><div><p class="eyebrow">The familiar depths. A few new tricks.</p><h2 id="features-title">An old evil.<br>A new lease on life.</h2></div><p>The dungeon comes first. Everything else helps you feel at home in it.</p></div>
  <div class="feature-grid">
    <article><span class="rune" aria-hidden="true">✦</span><h3>Forged for your Mac</h3><p>One universal build for Apple Silicon and Intel. Set resolution and fullscreen mode inside the game.</p></article>
    <article><span class="rune" aria-hidden="true">✥</span><h3>Your hand. Your rules.</h3><p>Mouse and keyboard, Xbox or DualSense. Remap buttons, rotate the camera and rule in comfort.</p></article>
    <article><span class="rune" aria-hidden="true">◆</span><h3>Guard your progress</h3><p>Manage saves, keep rotating backups and carry your settings with your player profile.</p></article>
    <article><span class="rune" aria-hidden="true">⚔</span><h3>Find a worthy rival</h3><p>Keep LAN and direct play, or join the community with public servers, accounts and match history.</p></article>
  </div>
</section>
<section class="network-hall wrap" aria-labelledby="network-title"><div class="section-heading"><div><p class="eyebrow">Beyond your dungeon walls</p><h2 id="network-title">Other keepers await.</h2></div><p>The website and game share one network. Find a server, connect your account or see what’s happening underground.</p></div><div class="hall-grid"><a href="/servers"><span class="hall-symbol" aria-hidden="true">⚔</span><h3>The server hall</h3><p>Browse public and dedicated games.</p><span class="hall-link">Browse servers</span></a><a href="/profile"><img src="/assets/keeper-sigil.svg" alt="" width="44" height="48"><h3>Your Keeper</h3><p>Account, friends and verified statistics.</p><span class="hall-link">Enter your profile</span></a><a href="/services"><span class="hall-symbol" aria-hidden="true">✥</span><h3>The network chamber</h3><p>Service status and the HTTP API browser.</p><span class="hall-link">Explore services</span></a></div><p class="hall-footnote"><a href="/api-browser">API browser</a><span>·</span><a href="/events">Events</a><span>·</span><a href="/replays">Replays</a><span>·</span><a href="/challenges">Cooperative challenges</a></p></section>
<section class="getting-started wrap" aria-labelledby="start-title"><div><p class="eyebrow">Your descent begins here</p><h2 id="start-title">Claim your dungeon.</h2><p>You’ll need your own copy of Dungeon Keeper. Original graphics, sound and game data are imported, rather than bundled with KeeperFX-E.</p></div><ol class="steps"><li><div><h3>Get KeeperFX-E</h3><p>Choose a build from the <a href="https://github.com/retrocs-org/KeeperFX-E/releases">release page</a>, or build from source.</p></div></li><li><div><h3>Import your game files</h3><p>Use the included import tools with your own Dungeon Keeper installation.</p></div></li><li><div><h3>Make yourself at home</h3><p>Choose your controls and display settings. Start a dungeon, or find a game.</p></div></li></ol></section>
<section class="project-links wrap" aria-labelledby="project-title"><div><p class="eyebrow">Built in the open</p><h2 id="project-title">Help shape the depths.</h2><p>Read the code, report a problem or contribute your next wicked idea.</p></div><div class="repository-links"><a href="https://github.com/retrocs-org/KeeperFX-E"><span>GitHub<small>Code, issues &amp; releases</small></span></a></div></section>
</main><?php site_footer(); ?></body></html>
