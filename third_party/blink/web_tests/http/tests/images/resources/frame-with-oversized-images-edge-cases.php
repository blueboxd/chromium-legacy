<?php
header("Document-Policy: oversized-images;scale=2.0");
?>
<!DOCTYPE html>
<body>
<img src="green-256x256.jpg">
<img src="green-256x256.jpg" width="128" height="128" >
<img src="green-256x256.jpg" width="127" height="127" >
<img src="green-256x256.jpg" width="129" height="129" >
</body>
