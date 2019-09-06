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
  -dir bundled_exchanges_browsertest/ \
  -manifestURL https://test.example.org/manifest.webmanifest \
  -o bundled_exchanges_browsertest.wbn
