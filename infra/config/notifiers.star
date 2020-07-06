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

luci.tree_closer(
    name = 'close-on-any-step-failure',
    tree_status_host = 'chromium-status.appspot.com',
)

def tree_closure_notifier(name, notify_emails):
  return luci.notifier(
      name = name,
      notify_emails = notify_emails,
      on_occurrence = ['FAILURE'],
      failed_step_regexp = TREE_CLOSING_STEPS,
  )

tree_closure_notifier(
    name = 'chromium-tree-closer-email',
    # Stand-in for the chromium build sheriff, while testing.
    notify_emails = ['orodley+test-tree-closing-notifier@chromium.org'],
)

tree_closure_notifier(
    name = 'gpu-tree-closer-email',
    # Stand-in for chrome-gpu-build-failures@google.com and the GPU build
    # sheriff, while testing.
    notify_emails = ['orodley+gpu-test-tree-closing-notifier@chromium.org'],
)

tree_closure_notifier(
    name = 'linux-memory',
    notify_emails = ['thomasanderson@chromium.org'],
)

tree_closure_notifier(
    name = 'linux-archive-rel',
    notify_emails = ['thomasanderson@chromium.org'],
)

tree_closure_notifier(
    name = 'Deterministic Android',
    notify_emails = ['agrieve@chromium.org'],
)

tree_closure_notifier(
    name = 'Deterministic Linux',
    notify_emails = [
        "tikuta@chromium.org",
        "ukai@chromium.org",
        "yyanagisawa@chromium.org",
    ],
)

tree_closure_notifier(
    name = 'linux-ozone-rel',
    notify_emails = [
        "fwang@chromium.org",
        "maksim.sisov@chromium.org",
        "rjkroege@chromium.org",
        "thomasanderson@chromium.org",
        "timbrown@chromium.org",
        "tonikitoo@chromium.org",
    ],
)

luci.notifier(
    name = 'Closure Compilation Linux',
    notify_emails = [
        "dbeam+closure-bots@chromium.org",
        "fukino+closure-bots@chromium.org",
        "hirono+closure-bots@chromium.org",
        "vitalyp@chromium.org",
    ],
    on_occurrence = ['FAILURE'],
    failed_step_regexp = [
        "update_scripts",
        "setup_build",
        "bot_update",
        "generate_gyp_files",
        "compile",
        "generate_v2_gyp_files",
        "compile_v2"
    ]
)

luci.notifier(
    name = 'Site Isolation Android',
    notify_emails = [
        "nasko+fyi-bots@chromium.org",
        "creis+fyi-bots@chromium.org",
        "lukasza+fyi-bots@chromium.org",
        "alexmos+fyi-bots@chromium.org",
    ],
    on_new_status = ['FAILURE'],
)

luci.notifier(
    name = 'CFI Linux',
    notify_emails = [
        'pcc@chromium.org',
    ],
    on_new_status = ['FAILURE'],
)

luci.notifier(
    name = 'Win 10 Fast Ring',
    notify_emails = [
        'wfh@chromium.org',
    ],
    on_new_status = ['FAILURE'],
)

luci.notifier(
    name = 'linux-blink-heap-verification',
    notify_emails = [
        'mlippautz+fyi-bots@chromium.org',
    ],
    on_new_status = ['FAILURE'],
)

luci.notifier(
    name = 'annotator-rel',
    notify_emails = [
        "pastarmovj@chromium.org",
        "nicolaso@chromium.org",
    ],
    on_new_status = ['FAILURE'],
)

tree_closure_notifier(
    name = 'chromium.linux',
    notify_emails = [
        'thomasanderson@chromium.org',
    ],
)
