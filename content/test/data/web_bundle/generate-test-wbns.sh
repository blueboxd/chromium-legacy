#!/bin/sh

# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

if ! command -v gen-bundle > /dev/null 2>&1; then
    echo "gen-bundle is not installed. Please run:"
    echo "  go get -u github.com/WICG/webpackage/go/bundle/cmd/..."
    exit 1
fi

gen-bundle \
  -version b1 \
  -baseURL https://test.example.org/ \
  -primaryURL https://test.example.org/ \
  -dir web_bundle_browsertest/ \
  -manifestURL https://test.example.org/manifest.webmanifest \
  -o web_bundle_browsertest.wbn

# Generate a base WBN which will used to generate broken WBN.
# This WBN must contains 3 entries:
#   [1]: https://test.example.org/
#   [2]: https://test.example.org/index.html
#   [3]: https://test.example.org/script.html
gen-bundle \
  -version b1 \
  -baseURL https://test.example.org/ \
  -primaryURL https://test.example.org/ \
  -dir broken_bundle/ \
  -o broken_bundle_base.wbn

# Rewrite ":status" (3a737461747573) header of the first entry to ":xxxxxx"
# (3a787878787878).
xxd -p broken_bundle_base.wbn |
  tr -d '\n' |
  sed 's/3a737461747573/3a787878787878/' |
  xxd -r -p > broken_bundle_broken_first_entry.wbn

# Rewrite ":status" (3a737461747573) header of the third entry (script.js) to
# ":xxxxxx" (3a787878787878).
xxd -p broken_bundle_base.wbn |
  tr -d '\n' |
  sed 's/3a737461747573/3a787878787878/3' |
  xxd -r -p > broken_bundle_broken_script_entry.wbn


# Keep consistent with WebBundleNetworkBrowserTest::kNetworkTestPort.
NETWORK_TEST_PORT=39600

gen-bundle \
  -version b1 \
  -baseURL http://localhost:$NETWORK_TEST_PORT/web_bundle/network/ \
  -primaryURL http://localhost:$NETWORK_TEST_PORT/web_bundle/network/ \
  -dir web_bundle_browsertest/ \
  -o web_bundle_browsertest_network.wbn

gen-bundle \
  -version b1 \
  -baseURL http://localhost:$NETWORK_TEST_PORT/web_bundle/network/ \
  -primaryURL http://localhost:$NETWORK_TEST_PORT/web_bundle/network/not_found.html \
  -dir web_bundle_browsertest/ \
  -o web_bundle_browsertest_network_primary_url_not_found.wbn

gen-bundle \
  -version b1 \
  -baseURL http://localhost:$NETWORK_TEST_PORT/web_bundle/path_test/ \
  -primaryURL http://localhost:$NETWORK_TEST_PORT/web_bundle/path_test/in_scope/ \
  -dir path_test_wbn/ \
  -o path_test.wbn

gen-bundle \
  -version b1 \
  -primaryURL https://test.example.org/ \
  -har variants_test.har \
  -o variants_test.wbn
