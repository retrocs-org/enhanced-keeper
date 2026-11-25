<?php
/** Keeper-Enhanced public website and native KeeperHub HTTP API. PHP 8.1+. */
declare(strict_types=1);
ini_set('display_errors','0');
umask(0077);

header('Cache-Control: no-store');
header('X-Content-Type-Options: nosniff');
header('Referrer-Policy: no-referrer');
header("Content-Security-Policy: default-src 'self'; script-src 'self'; style-src 'self'; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'");

function problem(int $status, string $message): never {
    http_response_code($status);
    header('Content-Type: application/json; charset=utf-8');
    echo json_encode(['error' => $message]);
    exit;
}

define('KFX_SETTINGS_INTERNAL', true);
require_once __DIR__.'/lib/settings.php';
try { $origin = web_setting('public_origin','KFX_PUBLIC_ORIGIN'); }
catch (Throwable $e) { problem(503,'Website configuration unavailable.'); }
$public = parse_url($origin);
if (!$public || !isset($public['scheme'], $public['host']) || (isset($public['user']) || isset($public['pass']))
    || isset($public['query']) || isset($public['fragment']) || !empty($public['path'])
    || !in_array($public['scheme'], ['http', 'https'], true)
    || ($public['scheme'] === 'http' && !in_array($public['host'], ['127.0.0.1', 'localhost', '[::1]'], true))) {
    problem(503, 'Website origin is not configured correctly.');
}
$authority = $public['host'] . (isset($public['port']) ? ':' . $public['port'] : '');
if (strtolower($_SERVER['HTTP_HOST'] ?? '') !== strtolower($authority)) {
    problem(403, 'Unexpected website host.');
}
if (isset($_SERVER['HTTP_ORIGIN']) && $_SERVER['HTTP_ORIGIN'] !== $origin) {
    problem(403, 'Cross-origin request rejected.');
}
$uri = $_SERVER['REQUEST_URI'] ?? '/';
if (preg_match('/[\x00-\x20\x7f]/', $uri)) {
    problem(400, 'Invalid request path.');
}
// Direct front-controller URLs work even when the host ignores .htaccess.
$requestPath = parse_url($uri, PHP_URL_PATH);
if (array_key_exists('route', $_GET)) {
    if (!in_array($requestPath, ['/', '/index.php'], true) || !is_string($_GET['route'])) problem(400, 'Invalid route');
    $uri = $_GET['route'];
    if (strlen($uri)>2048 || !str_starts_with($uri,'/') || str_starts_with($uri,'//')
        || str_contains($uri,'\\') || str_contains($uri,'#') || preg_match('/[\x00-\x20\x7f]/',$uri)) problem(400,'Invalid route');
}
$path = parse_url($uri, PHP_URL_PATH);
$method = $_SERVER['REQUEST_METHOD'] ?? 'GET';
if (!in_array($method, ['GET', 'POST'], true)) {
    header('Allow: GET, POST');
    problem(405, 'Method not allowed.');
}

if (is_string($path) && str_starts_with($path, '/api/')) {
    if (!preg_match('~^/api/[A-Za-z0-9/_-]+$~D', $path)) {
        problem(404, 'Not found.');
    }
    define('KFX_API_INTERNAL', true);
    require __DIR__.'/lib/http.php';
    serve_php_api($origin,$uri,$method);
}

if ($method !== 'GET') {
    header('Allow: GET');
    problem(405, 'Method not allowed.');
}
if (in_array($path, ['/assets/keeper-sigil.svg','/assets/dungeon-heart.svg','/assets/keeper-mask.svg','/assets/dungeon-stone.svg'], true)) {
    header('Content-Type: image/svg+xml; charset=utf-8');
    readfile(__DIR__.$path);
    exit;
}
if (in_array($path, ['/community-pages.js', '/routes.js', '/app.js', '/portal.js', '/api-browser.js', '/style.css', '/site.css'], true)) {
    header('Content-Type: ' . (str_ends_with($path,'.js') ? 'text/javascript' : 'text/css') . '; charset=utf-8');
    readfile(__DIR__ . $path);
    exit;
}
if ($path === '/api-catalog.json') { define('KFX_WEB_PAGE',true); require __DIR__.'/api-catalog.php'; header('Content-Type: application/json; charset=utf-8'); echo json_encode(api_catalog(),JSON_THROW_ON_ERROR); exit; }
// Keep old overview bookmarks working after removing the page.
if (in_array($path, ['/network', '/services', '/servers', '/network/servers', '/leaderboard'], true)) {
    $section=$path==='/leaderboard'?'rankings':(in_array($path,['/servers','/network/servers'],true)?'server-panel':'');
    header('Location: /index.php?route=%2Flair'.($section!==''?'#'.$section:''), true, 302);
    exit;
}
$path=['/network/servers'=>'/servers','/network/events'=>'/events','/network/replays'=>'/replays','/network/challenges'=>'/challenges'][$path]??$path;
if (!in_array($path, ['/', '/index.php', '/community', '/servers', '/events', '/profile', '/device', '/lair', '/api-browser', '/replays', '/challenges', '/features', '/leaderboard', '/downloads', '/credits'], true)
    && !(is_string($path) && preg_match('~^/keeper/[a-f0-9]{32}$~D', $path))) {
    problem(404, 'Not found.');
}
header('Content-Type: text/html; charset=utf-8');
define('KFX_WEB_PAGE', true);
require __DIR__.'/releases.php';
require __DIR__.'/layout.php';
require __DIR__.'/api-catalog.php';
if(in_array($path,['/features','/leaderboard','/downloads','/credits'],true)){require __DIR__.'/project.php';exit;}
require __DIR__ . (in_array($path,['/','/index.php'],true)?'/home.php':($path==='/api-browser'?'/network.php':'/community.php'));
