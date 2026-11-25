<?php
if (!defined('KFX_WEB_PAGE')) { http_response_code(404); exit; }
?>
<section class="features wrap<?= !empty($standaloneFeatures)?' is-page':'' ?>" id="features" aria-labelledby="features-title">
  <div class="section-heading"><div><p class="eyebrow">The KeeperFX foundation. Our additions.</p><?php if(!empty($standaloneFeatures)): ?><h1 id="features-title">What Keeper-Enhanced adds.</h1><?php else: ?><h2 id="features-title">What Keeper-Enhanced adds.</h2><?php endif; ?></div><p>Built on the work of KeeperFX and its macOS port, this fork focuses on native Mac play, controller support, recovery tools and connected multiplayer.</p></div>
  <div class="feature-grid">
    <article><span class="rune" aria-hidden="true">✦</span><h3>Native Mac experience</h3><p>One universal app for Apple Silicon and Intel. Import your original game files, then manage your setup from inside the game.</p></article>
    <article><span class="rune" aria-hidden="true">✥</span><h3>Xbox &amp; DualSense</h3><p>Remap buttons, calibrate sticks and use illustrated controller guides. Zoom and rotate the camera with your gamepad.</p></article>
    <article><span class="rune" aria-hidden="true">◈</span><h3>Display settings in-game</h3><p>Choose resolution, windowed or fullscreen mode and mouse confinement. Unconfirmed display changes revert; interrupted startup can recover to a safe window.</p></article>
    <article><span class="rune" aria-hidden="true">◆</span><h3>Save recovery tools</h3><p>Browse rotating backups and preserved saves, check engine and content requirements, and restore a compatible copy into a free slot.</p></article>
    <article><span class="rune" aria-hidden="true">⚔</span><h3>A connected community</h3><p>Browse public and dedicated servers, pair your account and follow verified match history. Use our service or your own; LAN and direct play remain available.</p></article>
    <article><span class="rune" aria-hidden="true">⌘</span><h3>Separate content profiles</h3><p>Keep different mod setups in separate profiles, each with its own runtime, saves and settings, while sharing your imported originals.</p></article>
    <article><span class="rune" aria-hidden="true">◷</span><h3>A replay library</h3><p>Organize recordings with titles and favourites. Pause playback, change speed, add bookmarks and export a replay without overwriting an existing file.</p></article>
    <article><span class="rune" aria-hidden="true">✧</span><h3>Comfort &amp; diagnostics</h3><p>Adjust caption and tooltip sizes, controller response and damage flashes. Inspect FPS, frame time and ping, or capture a short performance report.</p></article>
  </div>
  <p class="feature-details">Explore the <a href="<?= web_escape(web_url('/servers')) ?>">server browser</a> or read the <a href="https://github.com/retrocs-org/keeper-enhanced/blob/main/docs/POLISH-TOOLS.md">in-game tools guide</a>. Original Dungeon Keeper game files are still required.</p>
</section>
