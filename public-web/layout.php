<?php
if (!defined('KFX_WEB_PAGE')) { http_response_code(404); exit; }
function web_escape(string $text): string { return htmlspecialchars($text, ENT_QUOTES | ENT_SUBSTITUTE, 'UTF-8'); }
function network_url(string $path): string {
    return ['/services'=>'/lair','/community'=>'/lair','/servers'=>'/lair','/events'=>'/network/events','/replays'=>'/network/replays','/challenges'=>'/network/challenges'][$path]??$path;
}
function web_url(string $path): string {
    $path=network_url($path);
    return $path==='/' ? '/' : '/index.php?route='.rawurlencode($path);
}
function site_header(string $current = '/'): void {
    $current=network_url($current);
    $links=['/'=>'Overview','/features'=>'Features','/lair'=>'The Lair','/downloads'=>'Downloads'];
    echo '<a class="skip-link" href="#main">Skip to content</a><header class="site-header wrap"><a class="brand" href="/"><img class="brand-mark" src="/assets/keeper-mask.svg" width="52" height="58" alt=""><span>Keeper-Enhanced<small>A fork of KeeperFX</small></span></a><nav aria-label="Main navigation">';
    foreach($links as $url=>$label){
        $active=$current===$url;
        echo '<a href="'.web_escape(web_url($url)).'"'.($active?' aria-current="page"':'').($url==='/profile'?' data-account-label':'').'>'.$label.'</a>';
    }
    echo '</nav><a class="account-link" href="'.web_escape(web_url('/profile')).'" data-account-link'.(in_array($current,['/profile','/device'],true)?' aria-current="page"':'').'><span data-account-label>Sign in</span><span class="profile-avatar nav-avatar" data-account-avatar aria-hidden="true" hidden></span></a></header>';
    if($current==='/lair'||str_starts_with($current,'/network/')){
        echo '<div class="wrap"><nav class="network-nav" aria-label="Community navigation">';
        foreach(['/lair'=>'The Lair','/network/events'=>'Events','/network/replays'=>'Replays','/network/challenges'=>'Challenges'] as $url=>$label)
            echo '<a href="'.web_escape(web_url($url)).'"'.($current===$url?' aria-current="page"':'').'>'.$label.'</a>';
        echo '</nav></div>';
    }
}
function site_footer(): void {
    echo '<footer class="site-footer wrap"><div><a class="brand" href="/">Keeper-Enhanced</a><p>Keeper-Enhanced is an independent fork of <a href="https://github.com/dkfans/keeperfx">KeeperFX</a>. Original game assets are imported from your own copy.</p></div><nav aria-label="Project links"><a href="/index.php?route=%2Fcredits">Credits</a><a href="/index.php?route=%2Fdevice" data-auth-only hidden>Connect game</a><a href="/index.php?route=%2Fnetwork%2Fchallenges">Challenges</a><a href="https://github.com/retrocs-org/keeper-enhanced">GitHub</a></nav></footer>';
}
