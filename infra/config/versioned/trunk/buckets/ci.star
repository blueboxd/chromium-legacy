load('//lib/builders.star', 'builder', 'cpu', 'defaults', 'goma', 'os')
load('//versioned/vars/ci.star', 'vars')

luci.bucket(
    name = vars.bucket.get(),
    acls = [
        acl.entry(
            roles = acl.BUILDBUCKET_READER,
            groups = 'all',
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_TRIGGERER,
            groups = 'project-chromium-ci-schedulers',
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_OWNER,
            groups = 'google/luci-task-force@google.com',
        ),
    ],
)


defaults.bucket.set(vars.bucket.get())
defaults.bucketed_triggers.set(True)


def android_builder(
    *,
    name,
    # TODO(tandrii): migrate to this gradually (current value of
    # goma.jobs.MANY_JOBS_FOR_CI is 500).
    # goma_jobs=goma.jobs.MANY_JOBS_FOR_CI
    goma_jobs=goma.jobs.J150,
    **kwargs):
  return builder(
      name = name,
      goma_jobs = goma_jobs,
      mastername = 'chromium.android',
      **kwargs
  )

android_builder(
    name = 'android-kitkat-arm-rel',
    goma_backend = goma.backend.RBE_PROD,
)

android_builder(
    name = 'android-marshmallow-arm64-rel',
    goma_backend = goma.backend.RBE_PROD,
)


def gpu_builder(*, name, **kwargs):
  return builder(
      name = name,
      mastername = 'chromium.gpu',
      **kwargs
  )

gpu_builder(
    name = 'Android Release (Nexus 5X)',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_builder(
    name = 'GPU Linux Builder',
    goma_backend = goma.backend.RBE_PROD,
)


# Many of the GPU testers are thin testers, they use linux VMS regardless of the
# actual OS that the tests are built for
def gpu_linux_ci_tester(*, name, **kwargs):
  return gpu_builder(
      name = name,
      cores = 2,
      os = os.LINUX_DEFAULT,
      **kwargs
  )

gpu_linux_ci_tester(
    name = 'Linux Release (NVIDIA)',
)


def linux_builder(*, name, goma_jobs = goma.jobs.MANY_JOBS_FOR_CI, **kwargs):
  return builder(
      name = name,
      goma_jobs = goma_jobs,
      mastername = 'chromium.linux',
      **kwargs
  )

linux_builder(
    name = 'Linux Builder',
    goma_backend = goma.backend.RBE_PROD,
)

linux_builder(
    name = 'Linux Tests',
)
