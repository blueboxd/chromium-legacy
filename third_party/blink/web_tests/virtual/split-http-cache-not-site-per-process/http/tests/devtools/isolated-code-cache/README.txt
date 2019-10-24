# This suite runs the tests in http/tests/devtools/isolated-code-cache with
# --enable-features=SplitCacheByNetworkIsolationKey
# --disable-site-isolation-trials
# This feature partitions the HTTP cache by network isolation key for improved
# security. Tracking bug: crbug.com/910708. This test disables site isolation,
# which would cause similar results.
