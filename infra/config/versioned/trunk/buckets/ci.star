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


XCODE_IOS_11_CACHE = swarming.cache(
    name = 'xcode_ios_11a1027',
    path = 'xcode_ios_11a1027.app',
)


# Builders appear after the function used to define them, with all builders
# defined using the same function ordered lexicographically by name
# Builder functions are defined in lexicographic order by name ignoring the
# '_builder' suffix

# Builder functions are defined for each master, with additional functions
# for specializing on OS or particular configuration (e.g. builders running
# libfuzzer recipe): XXX_builder and XXX_YYY_builder where XXX is the part after
# the last dot in the mastername and YYY is the OS or configuration

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


def chromiumos_builder(*, name, **kwargs):
  return builder(
      name = name,
      mastername = 'chromium.chromiumos',
      goma_backend = goma.backend.RBE_PROD,
      **kwargs
  )

chromiumos_builder(
    name = 'chromeos-amd64-generic-rel',
    goma_enable_ats = True,
)

chromiumos_builder(
    name = 'linux-chromeos-rel',
)


def fyi_builder(
    *,
    name,
    execution_timeout=10 * time.hour,
    **kwargs):
  return builder(
      name = name,
      execution_timeout = execution_timeout,
      mastername = 'chromium.fyi',
      **kwargs
  )

# This is launching & collecting entirely isolated tests.
# OS shouldn't matter.
fyi_builder(
    name = 'mac-osxbeta-rel',
    goma_backend = goma.backend.RBE_PROD,
)


def fyi_windows_builder(*, name, os=os.WINDOWS_DEFAULT, **kwargs):
  return fyi_builder(
      name = name,
      os = os,
      **kwargs
  )

fyi_windows_builder(
    name = 'Win10 Tests x64 1803',
    os = os.WINDOWS_10,
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

gpu_builder(
    name = 'GPU Mac Builder',
    cores = None,
    goma_backend = goma.backend.RBE_PROD,
    os = os.MAC_ANY,
)

gpu_builder(
    name = 'GPU Win x64 Builder',
    builderless = True,
    os = os.WINDOWS_ANY,
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

gpu_linux_ci_tester(
    name = 'Mac Release (Intel)',
)

gpu_linux_ci_tester(
    name = 'Mac Retina Release (AMD)',
)

gpu_linux_ci_tester(
    name = 'Win10 x64 Release (NVIDIA)',
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


def mac_builder(
    *,
    name,
    cores=None,
    goma_backend = goma.backend.RBE_PROD,
    os=os.MAC_DEFAULT,
    **kwargs):
  return builder(
      name = name,
      cores = cores,
      goma_backend = goma_backend,
      mastername = 'chromium.mac',
      os = os,
      **kwargs
  )

mac_builder(
    name = 'Mac Builder',
)

# The build runs on 10.13, but triggers tests on 10.10 bots.
mac_builder(
    name = 'Mac10.10 Tests',
)

# The build runs on 10.13, but triggers tests on 10.11 bots.
mac_builder(
    name = 'Mac10.11 Tests',
)

mac_builder(
    name = 'Mac10.12 Tests',
    os = os.MAC_10_12,
)

mac_builder(
    name = 'Mac10.13 Tests',
    os = os.MAC_10_13,
)

mac_builder(
    name = 'WebKit Mac10.13 (retina)',
    os = os.MAC_10_13,
)


def mac_ios_builder(*, name, **kwargs):
  return mac_builder(
      name = name,
      caches = [XCODE_IOS_11_CACHE],
      executable = luci.recipe(name = 'ios/unified_builder_tester'),
      goma_backend = None,
      os = os.MAC_ANY,
      **kwargs
  )

mac_ios_builder(
    name = 'ios-simulator',
)


def win_builder(*, name, os=os.WINDOWS_DEFAULT, **kwargs):
  return builder(
      name = name,
      mastername = 'chromium.win',
      os = os,
      **kwargs
  )

win_builder(
    name = 'Win 7 Tests x64 (1)',
    os = os.WINDOWS_7,
)

win_builder(
    name = 'Win x64 Builder',
    cores = 32,
    os = os.WINDOWS_ANY,
)

win_builder(
    name = 'Win10 Tests x64',
)
