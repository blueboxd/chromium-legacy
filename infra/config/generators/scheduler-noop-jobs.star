# Don't make a habit of this - it isn't public API
load('@stdlib//internal/luci/proto.star', 'scheduler_pb')

# Android testers which are triggered by Android arm Builder (dbg)
# on master, but not on branches.
_ANDROID_NON_BRANCHED_TESTERS = (
    'Android WebView L (dbg)',
    'KitKat Phone Tester (dbg)',
    'KitKat Tablet Tester',
    'Lollipop Phone Tester',
    'Lollipop Tablet Tester',
    'Marshmallow Tablet Tester',
)
_ANDROID_TEST_NOOP_JOBS = [scheduler_pb.Job(
    id = bucket + '-' + builder,
    schedule = 'triggered',
    acl_sets = [bucket],
    acls = [scheduler_pb.Acl(
        role = scheduler_pb.Acl.TRIGGERER,
        granted_to = 'chromium-ci-builder@chops-service-accounts.iam.gserviceaccount.com',
    )],
    noop = scheduler_pb.NoopTask(),
) for builder in _ANDROID_NON_BRANCHED_TESTERS for bucket in (
    'ci-m80',
    'ci-m81',
)]


def _add_noop_jobs(ctx):
  cfg = ctx.output['luci-scheduler.cfg']
  for j in _ANDROID_TEST_NOOP_JOBS:
    cfg.job.append(j)

lucicfg.generator(_add_noop_jobs)
