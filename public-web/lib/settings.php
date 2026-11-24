<?php
/** One configuration entry point for the standalone public-web payload. */
if (!defined('KFX_API_INTERNAL') && !defined('KFX_SETTINGS_INTERNAL')) { http_response_code(404); exit; }
if (!defined('KFX_SETTINGS_INTERNAL')) define('KFX_SETTINGS_INTERNAL', true);
function web_settings(): array {
    static $settings;
    if ($settings !== null) return $settings;
    $file=dirname(__DIR__).'/config.inc.php';
    if (!is_file($file) || is_link($file) || filesize($file)>32768) throw new RuntimeException('Website configuration unavailable');
    $settings=require $file;
    if (!is_array($settings)) throw new RuntimeException('Invalid website configuration');
    return $settings;
}
function web_setting(string $name, string $environment, string $default=''): string {
    $override=getenv($environment);
    if ($override!==false && $override!=='') return $override;
    $value=web_settings()[$name]??$default;
    if (!is_string($value)) throw new RuntimeException('Invalid website setting');
    return $value;
}
