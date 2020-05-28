<?php
header("Document-Policy: oversized-images;scale=2.0");
?>
<!DOCTYPE html>
<body>
<!-- Note: give each image a unique URL so that the report generated
     will not be deduped. -->
<img src="green-256x256.jpg?id=1" width="128" height="128" style="border: 10px solid red;">
<img src="green-256x256.jpg?id=2" width="120" height="120" style="border: 10px solid red;">
<img src="green-256x256.jpg?id=3" width="120" height="120" style="padding: 10px;">
<img src="green-256x256.jpg?id=4" width="120" height="120" style="border: 10px solid red; padding: 5px;">
</body>
