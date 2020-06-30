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

TREE_CLOSING_STEPS = [
    'bot_update',
    'compile',
    'gclient runhooks',
    'runhooks',
    'update',
]

luci.tree_closer(
    name = 'chromium-tree-closer',
    tree_status_host = 'chromium-status.appspot.com',
    failed_step_regexp = TREE_CLOSING_STEPS
)

luci.notifier(
    name = 'chromium-tree-closer-email',
    on_occurrence = ['FAILURE'],
    # Stand-in for the chromium build sheriff, while testing.
    notify_emails = ['orodley+test-tree-closing-notifier@chromium.org'],
    failed_step_regexp = TREE_CLOSING_STEPS,
)

luci.notifier(
    name = 'gpu-tree-closer-email',
    on_occurrence = ['FAILURE'],
    # Stand-in for chrome-gpu-build-failures@google.com and the GPU build
    # sheriff, while testing.
    notify_emails = ['orodley+gpu-test-tree-closing-notifier@chromium.org'],
    failed_step_regexp = TREE_CLOSING_STEPS,
)
