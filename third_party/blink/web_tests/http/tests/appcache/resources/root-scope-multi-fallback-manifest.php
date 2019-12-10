<?php
require_once '../../resources/portabilityLayer.php';

header("Expires: Thu, 01 Dec 2003 16:00:00 GMT");
header("Cache-Control: no-cache, must-revalidate");
header("Pragma: no-cache");
header("Content-Type: text/cache-manifest");
header("X-AppCache-Allowed: /");

print("CACHE MANIFEST\n");
print("FALLBACK:\n");
print("/resources/network-simulator.php? simple.txt\n");
print("does-not empty.txt\n");
print("does-not-exist simple.txt\n");
?>
