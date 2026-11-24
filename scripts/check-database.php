<?php
/** Compatibility wrapper; the deployable check lives in public-web. */
if(PHP_SAPI!=='cli')exit(1);
require dirname(__DIR__).'/public-web/check-database.php';
