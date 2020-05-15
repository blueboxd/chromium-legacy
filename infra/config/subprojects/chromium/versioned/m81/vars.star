# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

vars = struct(
    is_master = False,
    ref = 'refs/branch-heads/4044',
    ci_bucket = 'ci-m81',
    ci_poller = 'm81-gitiles-trigger',
    main_console_name = 'main-m81',
    main_console_title = 'Chromium M81 Console',
    try_bucket = 'try-m81',
    cq_group = 'cq-m81',
    cq_ref_regexp = 'refs/branch-heads/4044',
    main_list_view_name = 'try-m81',
    main_list_view_title = 'Chromium M81 CQ console',
    tree_status_host = None,
)
