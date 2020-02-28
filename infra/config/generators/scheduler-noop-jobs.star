# Don't make a habit of this - it isn't public API
load('@stdlib//internal/luci/proto.star', 'scheduler_pb')

_GPU_NOOP_JOBS = [scheduler_pb.Job(
    id = builder,
    schedule = 'triggered',
    acl_sets = ['ci'],
    acls = [scheduler_pb.Acl(
        role = scheduler_pb.Acl.TRIGGERER,
        granted_to = 'chromium-ci-gpu-builder@chops-service-accounts.iam.gserviceaccount.com',
    )],
    noop = scheduler_pb.NoopTask(),
) for builder in (
    'ANGLE GPU Linux Release (Intel HD 630)',
    'ANGLE GPU Linux Release (NVIDIA)',
    'ANGLE GPU Mac Release (Intel)',
    'ANGLE GPU Mac Retina Release (AMD)',
    'ANGLE GPU Mac Retina Release (NVIDIA)',
    'ANGLE GPU Win10 x64 Release (Intel HD 630)',
    'ANGLE GPU Win10 x64 Release (NVIDIA)',
    'Optional Linux Release (Intel HD 630)',
    'Optional Linux Release (NVIDIA)',
    'Optional Mac Release (Intel)',
    'Optional Mac Retina Release (AMD)',
    'Optional Mac Retina Release (NVIDIA)',
    'Optional Win10 x64 Release (Intel HD 630)',
    'Optional Win10 x64 Release (NVIDIA)',
    'Win7 ANGLE Tryserver (AMD)',
)]

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
    'ci-beta',
    'ci-stable',
)]


def _add_noop_jobs(ctx):
  cfg = ctx.output['luci-scheduler.cfg']
  for j in _GPU_NOOP_JOBS + _ANDROID_TEST_NOOP_JOBS:
    cfg.job.append(j)

def _munge_trunk_only_jobs(ctx):
  cfg = ctx.output['luci-scheduler.cfg']
  for j in cfg.job:
    if j.id in _ANDROID_NON_BRANCHED_TESTERS:
      j.id = 'ci-' + j.id

lucicfg.generator(_add_noop_jobs)
lucicfg.generator(_munge_trunk_only_jobs)
