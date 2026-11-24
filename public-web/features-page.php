<?php
if (!defined('KFX_WEB_PAGE')) { http_response_code(404); exit; }
?>
<header class="feature-intro wrap">
  <p class="eyebrow">The game / the engine / our fork</p>
  <h1>From Dungeon Keeper<br>to KeeperFX Enhanced.</h1>
  <p class="lead">Classic dungeon strategy, with modern controls and more ways to play together.</p>
</header>
<section class="feature-lineage wrap" id="lineage" aria-labelledby="lineage-title">
  <p class="eyebrow">A shared history</p>
  <h2 id="lineage-title">Three chapters. One dungeon.</h2>
  <ol class="simple-timeline">
    <li><time datetime="1997">1997</time><span class="timeline-node" aria-hidden="true"></span><p class="timeline-label">Original game</p><h3>Dungeon Keeper</h3><p>Build rooms, attract creatures, cast spells and defend your Dungeon Heart. Bullfrog’s original game sets the foundation.</p><a href="https://store.steampowered.com/app/1996630/Dungeon_Keeper_Gold/">Dungeon Keeper Gold on Steam ↗</a></li>
    <li><time datetime="2008">2008</time><span class="timeline-node" aria-hidden="true"></span><p class="timeline-label">Open-source remake</p><h3>KeeperFX</h3><p>A community remake with engine fixes, higher resolutions, new campaigns, scripting, mods and multiplayer.</p><a href="https://keeperfx.net/">Explore KeeperFX ↗</a></li>
    <li class="timeline-enhanced"><time datetime="2026">2026</time><span class="timeline-node" aria-hidden="true"></span><p class="timeline-label">Our community fork</p><h3>KeeperFX Enhanced</h3><p>Native Mac play, integrated settings, save recovery and connected multiplayer with private and public dedicated hosts.</p><a href="#enhanced">See our features ↓</a></li>
  </ol>
</section>
<section class="feature-additions wrap" id="enhanced" aria-labelledby="enhanced-title">
  <p class="eyebrow">KeeperFX Enhanced</p>
  <h2 id="enhanced-title">Make the dungeon yours.</h2>
  <p class="feature-summary">Built on KeeperFX, our fork focuses on a comfortable Mac experience, recovery tools and community play.</p>
  <div class="simple-feature-grid">
    <article><p class="eyebrow">Mac &amp; controls</p><h3>Play your way.</h3><p>A universal Mac app for Apple Silicon and Intel. Set resolution, fullscreen mode and mouse confinement inside the game. Remap Xbox and DualSense controls, calibrate sticks, and zoom or rotate the camera with your gamepad.</p><a href="<?= web_escape(web_url('/downloads')) ?>">Get the game</a></article>
    <article><p class="eyebrow">Saves &amp; replays</p><h3>Keep your progress.</h3><p>Browse save backups, check compatibility and restore a suitable copy into a free slot. Keep mod setups in separate profiles, organize your replay library, and pause, change playback speed or bookmark a moment.</p><a href="https://github.com/retrocs-org/KeeperFX-E/blob/main/docs/POLISH-TOOLS.md">Explore the in-game tools ↗</a></article>
    <article><p class="eyebrow">Multiplayer &amp; community</p><h3>Find your next game.</h3><p>Play on LAN, connect directly to a private server or browse listed public servers. Use the same directory in-game and on the website. Pair an optional account for match history and public verified statistics, or configure your own community service.</p><a href="<?= web_escape(web_url('/network')) ?>">Explore the network</a></article>
    <article><p class="eyebrow">Dedicated hosting</p><h3>A dungeon that stays ready.</h3><p>Run an unlisted private host or a listed public server, with or without a password. The headless supervisor rotates maps and records matches. Linux build tooling, a data importer and a systemd template are included.</p><a href="https://github.com/retrocs-org/KeeperFX-E/blob/main/docs/LINUX-SERVER.md">Dedicated-server setup ↗</a></article>
  </div>
  <p class="feature-footnote">Dedicated hosting supports up to three humans plus one AI host. Linux build and runtime validation are pending. Requires macOS 13+ for the Mac app and your own original Dungeon Keeper game files.</p>
</section>
<section class="feature-foundations wrap" id="original-game" aria-labelledby="original-title">
  <h2 id="original-title">The original Dungeon Keeper</h2>
  <p>Carve out a dungeon, manage gold, feed and train your creatures, then prepare for the heroes at your door. Pick up minions, set traps and possess a creature to see your domain from inside. The original graphics, sounds and game data come from your own copy.</p>
  <a href="https://store.steampowered.com/app/1996630/Dungeon_Keeper_Gold/">Dungeon Keeper Gold on Steam ↗</a>
  <details><summary>Sources &amp; project notes</summary><p>Dates and project history: <a href="https://keeperfx.net/history">KeeperFX history</a>. KeeperFX’s Unofficial branch returned to the main project in 2020; this overview groups that history under KeeperFX. <a href="https://github.com/Loobinex/keeperfx-unofficial">Read the branch announcement</a>.</p><p>KeeperFX continues independently. Controllers, multiplayer and modding also exist upstream; this page describes our fork’s particular workflows. KeeperFX Enhanced is an independent fork of KeeperFX, not an official Bullfrog or EA release.</p><p>See our <a href="https://github.com/retrocs-org/KeeperFX-E/blob/main/docs/RELEASE-NOTES.md">release notes</a> for implementation details and current limitations.</p></details>
</section>
