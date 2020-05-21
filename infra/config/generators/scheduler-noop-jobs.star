# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Don't make a habit of this - it isn't public API
load('@stdlib//internal/luci/proto.star', 'scheduler_pb')
load('//project.star', 'settings')


_BRANCH_NOOP_CONFIG = struct(
    buckets = ['ci-m83', 'ci-m84'],
    fmt = '{bucket}-{builder}',
) if settings.is_master else struct(
    buckets = ['ci'],
    fmt = '{builder}',
)


# Android testers which are triggered by Android arm Builder (dbg)
# on master, but not on branches.
_ANDROID_NON_BRANCHED_TESTERS = (
    'Android WebView L (dbg)',
    'Lollipop Phone Tester',
    'Lollipop Tablet Tester',
    'Marshmallow Tablet Tester',
)


_ANDROID_TEST_NOOP_JOBS = [scheduler_pb.Job(
    id = _BRANCH_NOOP_CONFIG.fmt.format(bucket=bucket, builder=builder),
    schedule = 'triggered',
    acl_sets = [bucket],
    acls = [scheduler_pb.Acl(
        role = scheduler_pb.Acl.TRIGGERER,
        granted_to = 'chromium-ci-builder@chops-service-accounts.iam.gserviceaccount.com',
    )],
    noop = scheduler_pb.NoopTask(),
) for builder in _ANDROID_NON_BRANCHED_TESTERS for bucket in _BRANCH_NOOP_CONFIG.buckets]


def _add_noop_jobs(ctx):
  cfg = ctx.output['luci-scheduler.cfg']
  for j in _ANDROID_TEST_NOOP_JOBS:
    cfg.job.append(j)

lucicfg.generator(_add_noop_jobs)
