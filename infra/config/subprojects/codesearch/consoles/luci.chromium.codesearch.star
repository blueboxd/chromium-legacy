# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

luci.list_view(
    name = "luci.chromium.codesearch",
    entries = [
        "codesearch/gen-android-try",
        "codesearch/gen-chromiumos-try",
        "codesearch/gen-fuchsia-try",
        "codesearch/gen-lacros-try",
        "codesearch/gen-linux-try",
        "codesearch/gen-win-try",
    ],
)
