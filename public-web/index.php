<?php
/** KeeperFX-E public website and same-origin KeeperHub API gateway. PHP 8.1+. */
declare(strict_types=1);

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

$origin = getenv('KFX_PUBLIC_ORIGIN') ?: 'https://keeperfx-e.retrocs.org';
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
    $upstream = getenv('KFX_HUB_UPSTREAM') ?: 'http://127.0.0.1:8766';
    // The upstream is server configuration, never a URL supplied by a visitor.
    if (!preg_match('~^http://(?:127\.0\.0\.1|localhost|\[::1\]):[0-9]{1,5}$~D', $upstream)) {
        problem(503, 'Community service is not configured correctly.');
    }
    if ((int)($_SERVER['CONTENT_LENGTH'] ?? 0) > 3 * 1024 * 1024) {
        problem(413, 'Request too large.');
    }
    $body = file_get_contents('php://input', false, null, 0, 3 * 1024 * 1024 + 1);
    if ($body === false || strlen($body) > 3 * 1024 * 1024) {
        problem(413, 'Request too large.');
    }
    $headers = ['Host: ' . $authority, 'Content-Type: application/json', 'Connection: close'];
    if (filter_var($_SERVER['REMOTE_ADDR'] ?? '', FILTER_VALIDATE_IP)) {
        $headers[] = 'X-KFX-Client-IP: ' . $_SERVER['REMOTE_ADDR'];
    }
    foreach (['HTTP_COOKIE' => 'Cookie', 'HTTP_AUTHORIZATION' => 'Authorization',
              'HTTP_X_CSRF_TOKEN' => 'X-CSRF-Token', 'HTTP_ORIGIN' => 'Origin'] as $key => $name) {
        if (isset($_SERVER[$key])) {
            $value = $_SERVER[$key];
            if (preg_match('/[\r\n\x00]/', $value)) {
                problem(400, 'Invalid request header.');
            }
            $headers[] = $name . ': ' . $value;
        }
    }
    $context = stream_context_create(['http' => [
        'method' => $method, 'header' => implode("\r\n", $headers), 'content' => $body,
        'timeout' => 10, 'ignore_errors' => true, 'follow_location' => 0,
    ]]);
    $stream = @fopen($upstream . $uri, 'rb', false, $context);
    if ($stream === false) {
        problem(502, 'Community service is unavailable. Please try again.');
    }
    $metadata = stream_get_meta_data($stream);
    $response = stream_get_contents($stream, 4 * 1024 * 1024 + 1);
    $timedOut = stream_get_meta_data($stream)['timed_out'];
    fclose($stream);
    $responseHeaders = $metadata['wrapper_data'] ?? [];
    if ($timedOut || $response === false || strlen($response) > 4 * 1024 * 1024
        || !preg_match('~^HTTP/\S+ ([0-9]{3})~', $responseHeaders[0] ?? '', $status)
        || (int)$status[1] < 200 || (int)$status[1] >= 600
        || ((int)$status[1] >= 300 && (int)$status[1] < 400)) {
        problem(502, 'Invalid community service response.');
    }
    http_response_code((int)$status[1]);
    header('Content-Type: application/json; charset=utf-8');
    foreach ($responseHeaders as $line) {
        if (stripos($line, 'Set-Cookie:') === 0) {
            header($line, false);
        }
    }
    echo $response;
    exit;
}

if ($method !== 'GET') {
    header('Allow: GET');
    problem(405, 'Method not allowed.');
}
if (in_array($path, ['/app.js', '/style.css', '/site.css'], true)) {
    header('Content-Type: ' . ($path === '/app.js' ? 'text/javascript' : 'text/css') . '; charset=utf-8');
    readfile(__DIR__ . $path);
    exit;
}
if (!in_array($path, ['/', '/index.php', '/community', '/servers', '/events', '/profile', '/device'], true)
    && !(is_string($path) && preg_match('~^/keeper/[a-f0-9]{32}$~D', $path))) {
    problem(404, 'Not found.');
}
header('Content-Type: text/html; charset=utf-8');
define('KFX_WEB_PAGE', true);
require __DIR__.'/releases.php';
require __DIR__ . (in_array($path, ['/', '/index.php'], true) ? '/home.php' : '/community.php');
