<?php
if (!defined('KFX_WEB_PAGE')) { http_response_code(404); exit; }
function web_escape(string $text): string { return htmlspecialchars($text, ENT_QUOTES | ENT_SUBSTITUTE, 'UTF-8'); }
function web_url(string $path): string {
    return $path==='/' ? '/' : '/index.php?route='.rawurlencode($path);
}
function site_header(string $current = '/'): void {
    $links = ['/'=>'Overview','/servers'=>'Servers','/services'=>'Network','/events'=>'Events','/replays'=>'Replays','/profile'=>'My Keeper'];
    echo '<a class="skip-link" href="#main">Skip to content</a><header class="site-header wrap"><a class="brand" href="/"><img class="brand-mark" src="/assets/keeper-mask.svg" width="52" height="58" alt=""><span>KeeperFX-E<small>A fork of KeeperFX</small></span></a><nav aria-label="Main navigation">';
    foreach ($links as $url=>$label) echo '<a href="'.web_escape(web_url($url)).'"'.($current===$url?' aria-current="page"':'').'>'.$label.'</a>';
    echo '</nav></header>';
}
function site_footer(): void {
    echo '<footer class="site-footer wrap"><div><a class="brand" href="/">KeeperFX-E</a><p>KeeperFX-E is an independent fork of <a href="https://github.com/dkfans/keeperfx">KeeperFX</a>. Original game assets are imported from your own copy.</p></div><nav aria-label="Project links"><a href="/index.php?route=%2Fdevice">Connect game</a><a href="/index.php?route=%2Fchallenges">Challenges</a><a href="https://github.com/retrocs-org/KeeperFX-E">GitHub</a></nav></footer>';
}
