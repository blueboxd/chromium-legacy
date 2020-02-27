<?php

# Generate token with the command:
# generate_token.py http://127.0.0.1:8000 DocumentPolicy --expire-timestamp=2000000000
header("Origin-Trial: Ak4CsAXUdUgi3o77HXbvmBfDxj2vdzWBqqTl9/WEkfaVRowGsyVaMk3Vgn4AXtGJeOPfxf3E0Zh+WUOYHQOrcA0AAABWeyJvcmlnaW4iOiAiaHR0cDovLzEyNy4wLjAuMTo4MDAwIiwgImZlYXR1cmUiOiAiRG9jdW1lbnRQb2xpY3kiLCAiZXhwaXJ5IjogMjAwMDAwMDAwMH0=");
?>
<title>Document policy interface - enabled by origin trial</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
<body>
<script>
test(t => {
  var iframeInterfaceNames = Object.getOwnPropertyNames(this.HTMLIFrameElement.prototype);
  assert_in_array('policy', iframeInterfaceNames);
}, 'Document Policy `policy` attribute exists in origin-trial enabled document.');
</script>
</body>
