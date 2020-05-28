<?php
header("Document-Policy: oversized-images;scale=2.0");
?>
<!DOCTYPE html>
<head>
  <base href="resources/">
</head>
<body>
  <div width="600" height="500">
    <img src="green-256x256.jpg">
    <img src="green-256x256.jpg" width="100" height="256">
    <img src="green-256x256.jpg" style="height: 100px; width: 256px">
    <img src="green-256x256.jpg" width="128" height="128" >
    <img src="green-256x256.jpg" width="50" height="50">
    <img src="green-256x256.jpg" style="height: 50px; weight: 50px">
    <img src="green-256x256.jpg" style="height: 1cm; weight: 1cm">
    <img src="green-256x256.jpg" style="height: 1cm; weight: 1cm; border-radius: 5px; border: 1px solid blue;">
  </div>
</body>
