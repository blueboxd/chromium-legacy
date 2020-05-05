#!/bin/bash
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Updates the binary proto in components/test/data/feed to match the textproto
# here.
CHROMIUM_DIR=$(realpath $(dirname $(readlink -f $0))/../../../../..)

cat "$CHROMIUM_DIR/components/feed/core/v2/testdata/response.textproto" | \
  "$CHROMIUM_DIR/components/feed/core/v2/tools/encode_feed_response.sh" > \
  "$CHROMIUM_DIR/components/test/data/feed/response.binarypb"
