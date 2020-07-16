# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

luci.console_view(
    name = 'metadata.exporter',
    repo = 'https://chromium.googlesource.com/chromium/src',
    entries = [
        # TODO(crbug.com/1102997): remove this in favor of new
        # "metadata-exporter" builder.
        luci.console_view_entry(
            builder = 'ci/linux_chromium_component_updater',
        ),
        luci.console_view_entry(
            builder = 'ci/metadata-exporter',
        ),
    ],
)
