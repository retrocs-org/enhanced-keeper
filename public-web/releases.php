<?php
/** Read only updater-verified release metadata, stored outside the document root. */
function current_release(): ?array {
    $path = web_setting('release_manifest','KFX_RELEASE_MANIFEST');
    if (!is_file($path) || is_link($path) || filesize($path) > 32768) return null;
    $release = json_decode((string)file_get_contents($path), true);
    if (!is_array($release) || !is_string($release['version'] ?? null) || !preg_match('/^[0-9]+\.[0-9]+\.[0-9]+$/D', $release['version'] ?? '')) return null;
    if (!is_array($release['assets'] ?? null) || count($release['assets']) !== 2) return null;
    foreach ($release['assets'] as $asset) {
        if (!is_array($asset) || !is_string($asset['sha256'] ?? null) || !is_string($asset['url'] ?? null) || !is_string($asset['type'] ?? null) || !preg_match('/^[a-f0-9]{64}$/D', $asset['sha256'] ?? '')
            || !preg_match('~^https://github\.com/retrocs-org/keeper-enhanced/releases/download/[^/]+/KeeperFX-E-[0-9.]+-macOS-Universal\.(?:dmg|zip)$~D', $asset['url'] ?? '')
            || !in_array($asset['type'] ?? '', ['dmg', 'zip'], true)) return null;
    }
    return $release;
}
