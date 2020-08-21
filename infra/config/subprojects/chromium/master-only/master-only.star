# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(gbeaty) external_console_view uses new fields/types that aren't present
# in the version of the protobuf that lint-luci-milo uses, so update protos and
# then uncomment these (the main console is still reachable via the beta link in
# the header)
# luci.external_console_view(
#     name = "main-m86",
#     title = "Chromium M86 Main Console",
#     source = "chromium-m86:main",
# )

# luci.external_console_view(
#     name = "mirrors-m86",
#     title = "Chromium M86 CQ Mirrors Console",
#     source = "chromium-m86:mirrors",
# )

# luci.external_console_view(
#     name = "try-m86",
#     title = "Chromium M86 CQ Console",
#     source = "chromium-m86:try",
# )

exec("./ci.star")
exec("./gpu.try.star")
exec("./swangle.try.star")
exec("./try.star")

exec("./consoles/android.packager.star")
exec("./consoles/luci.chromium.try.star")
exec("./consoles/metadata.exporter.star")
exec("./consoles/sheriff.ios.star")

exec("./fallback-cq.star")
