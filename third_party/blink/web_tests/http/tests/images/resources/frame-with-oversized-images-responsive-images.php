<?php
header("Document-Policy: oversized-images;scale=2.0");
?>
<!DOCTYPE html>
<body>
  <!-- Note: give each image a unique URL so that the report generated
      will not be deduped. -->
  <img src="green-256x256.jpg?id=1" intrinsicsize="100 x 100" width="127" height="127">
  <img srcset="green-256x256.jpg?id=2 256w" sizes="100px" width="127" height="127">
  <img srcset="green-256x256.jpg?id=3 256w" sizes="100px" width="128" height="128">
  <img srcset="green-256x256.jpg?id=4 256w" sizes="100px" width="129" height="129">
</body>
