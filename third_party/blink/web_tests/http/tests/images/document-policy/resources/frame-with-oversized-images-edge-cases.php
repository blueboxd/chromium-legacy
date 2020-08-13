<?php
header("Document-Policy: oversized-images=2.0");
?>
<!DOCTYPE html>

<!--
  Images should be replaced by placeholders if there are considered
  oversized, i.e. having
  image_size / (container_size * pixel_ratio) > threshold.
  Threshold is set by the document policy header(2.0 in this test).
-->
<body>
  <img src="green-256x256.jpg">
  <!-- Following cases are for device pixel ratio = 1.0 -->
  <!-- Image with size < 128 should all be replaced with placeholders -->
  <img src="green-256x256.jpg" width="128" height="128">
  <img src="green-256x256.jpg" width="127" height="127">
  <img src="green-256x256.jpg" width="129" height="129">
  <!-- Following cases are for device pixel ratio = 2.0 -->
  <!-- Image with size < 64 should all be replaced with placeholders -->
  <img src="green-256x256.jpg" width="64" height="64">
  <img src="green-256x256.jpg" width="63" height="63">
  <img src="green-256x256.jpg" width="65" height="65">
</body>