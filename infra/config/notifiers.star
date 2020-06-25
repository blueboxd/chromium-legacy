# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

luci.notifier(
    name = 'chromesec-lkgr-failures',
    on_status_change = True,
    notify_emails = [
        'chromesec-lkgr-failures@google.com',
    ],
)

luci.notifier(
    name = 'chrome-memory-sheriffs',
    on_status_change = True,
    notify_emails = [
        'chrome-memory-sheriffs+bots@google.com',
    ],
)

luci.notifier(
    name = 'cr-fuchsia',
    on_status_change = True,
    notify_emails = [
        'cr-fuchsia+bot@chromium.org',
    ],
)

luci.notifier(
    name = 'cronet',
    on_status_change = True,
    notify_emails = [
        'cronet-bots-observer@google.com',
    ],
)

luci.notifier(
    name = 'component-mapping',
    on_new_status = ['FAILURE'],
    notify_emails = ['chromium-component-mapping@google.com'],
)
