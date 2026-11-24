<?php
if (!defined('KFX_WEB_PAGE')) { http_response_code(404); exit; }
function web_escape(string $text): string { return htmlspecialchars($text, ENT_QUOTES | ENT_SUBSTITUTE, 'UTF-8'); }
function site_header(string $current = '/'): void {
    $links = ['/'=>'Overview','/servers'=>'Servers','/services'=>'Services','/api-browser'=>'API browser','/events'=>'Events','/replays'=>'Replays','/profile'=>'My Keeper'];
    echo '<a class="skip-link" href="#main">Skip to content</a><header class="site-header wrap"><a class="brand" href="/"><span class="brand-mark" aria-hidden="true">E</span>KeeperFX-E</a><nav aria-label="Main navigation">';
    foreach ($links as $url=>$label) echo '<a href="'.$url.'"'.($current===$url?' aria-current="page"':'').'>'.$label.'</a>';
    echo '</nav></header>';
}
function site_footer(): void {
    echo '<footer class="site-footer wrap"><div><a class="brand" href="/">KeeperFX-E</a><p>An independent project based on KeeperFX. Original game assets are imported from your own copy.</p></div><nav aria-label="Project links"><a href="/device">Connect game</a><a href="/challenges">Challenges</a><a href="https://github.com/retrocs-org/KeeperFX-E">GitHub</a><a href="https://gitlab.kladhest.se/retrocs-org/keeperfx-e">GitLab</a></nav></footer>';
}
