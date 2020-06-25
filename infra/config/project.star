# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

settings = struct(
    # Switch this to False for branches
    is_master = True,
    ref = 'refs/heads/master',
    ci_bucket = 'ci',
    ci_poller = 'master-gitiles-trigger',
    main_console_name = 'main',
    main_console_title = 'Chromium Main Console',
    cq_mirrors_console_name = 'mirrors',
    cq_mirrors_console_title = 'Chromium CQ Mirrors Console',
    try_bucket = 'try',
    try_triggering_projects = ['angle', 'dawn', 'skia', 'v8'],
    cq_group = 'cq',
    cq_ref_regexp = 'refs/heads/.+',
    main_list_view_name = 'try',
    main_list_view_title = 'Chromium CQ console',
    # Switch this to None for branches
    tree_status_host = 'chromium-status.appspot.com/',
)


def master_only_exec(f):
  if settings.is_master:
    exec(f)


# The branch numbers of branches that we have builders running for (including
# milestone-specific projects)
# Branch numbers for milestones can be viewed in the chromium column at
# https://chromiumdash.appspot.com/branches
# The 2 highest milestones will be the ones with active branches
ACTIVE_BRANCH_NUMBERS = [
    4103,
    4147,
]
