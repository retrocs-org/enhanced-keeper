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
function profile_sections(): array {
    return ['profile'=>'Profile','friends'=>'Friends','history'=>'Match history','security'=>'Account security'];
}
function profile_section(): string {
    $section=$_GET['section']??'profile';
    return is_string($section)&&isset(profile_sections()[$section])?$section:'profile';
}
function site_header(string $current = '/'): void {
    $current=network_url($current);
    $links=['/'=>'Overview','/features'=>'Features','/lair'=>'The Lair','/downloads'=>'Downloads'];
    echo '<a class="skip-link" href="#main">Skip to content</a><header class="site-header wrap"><a class="brand" href="/"><img class="brand-mark" src="/assets/keeper-mask.svg" width="52" height="58" alt=""><span>Keeper-Enhanced<small>A fork of KeeperFX</small></span></a><nav aria-label="Main navigation">';
    foreach($links as $url=>$label){
        $active=$current===$url;
        echo '<a href="'.web_escape(web_url($url)).'"'.($active?' aria-current="page"':'').($url==='/profile'?' data-account-label':'').'>'.$label.'</a>';
    }
    $profileUrl=web_escape(web_url('/profile'));
    echo '</nav><a class="account-link" href="'.$profileUrl.'" data-guest-only>Sign in</a>';
    echo '<details class="account-menu" data-auth-only hidden><summary aria-label="Keeper account"><span class="profile-avatar nav-avatar" data-account-avatar aria-hidden="true"></span></summary><ul>';
    foreach(profile_sections() as $section=>$label){
        $url=$profileUrl.($section==='profile'?'':'&amp;section='.$section);
        $active=$current==='/profile'&&profile_section()===$section;
        echo '<li><a href="'.$url.'"'.($active?' aria-current="page"':'').'>'.$label.'</a></li>';
    }
    echo '</ul></details></header>';
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
