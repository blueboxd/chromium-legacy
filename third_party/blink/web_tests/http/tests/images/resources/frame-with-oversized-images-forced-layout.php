<?php
header("Document-Policy: oversized-images;scale=2.0");
?>
<!DOCTYPE html>
<style>body { margin: 0; }</style>
<!-- Note: give each image a unique URL so that the report generated
      will not be deduped. -->
<img src="green-256x256.jpg?id=1" width="100" height="100">
<img src="green-256x256.jpg?id=2" style="width: 100px; height: 100px">
<script>
document.body.offsetTop;
</script>
