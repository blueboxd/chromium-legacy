"""Library for defining CI builders.

The `ci_builder` function defined in this module enables defining a CI builder.
It can also be accessed through `ci.builder`.

The `defaults` struct provides module-level defaults for the arguments to
`ci_builder`. The parameters that support module-level defaults have a
corresponding attribute on `defaults` that is a `lucicfg.var` that can be used
to set the default value. Can also be accessed through `ci.defaults`.
"""

load('./builders.star', 'builders')


defaults = builders.defaults


def ci_builder(*, name, **kwargs):
  """Define a CI builder.

  Arguments:
    * name - name of the builder, will show up in UIs and logs. Required.
  """
  return builders.builder(
      name = name,
      **kwargs
  )


def android_builder(
    *,
    name,
    # TODO(tandrii): migrate to this gradually (current value of
    # goma.jobs.MANY_JOBS_FOR_CI is 500).
    # goma_jobs=goma.jobs.MANY_JOBS_FOR_CI
    goma_jobs=builders.goma.jobs.J150,
    **kwargs):
  return ci_builder(
      name = name,
      goma_backend = builders.goma.backend.RBE_PROD,
      goma_jobs = goma_jobs,
      mastername = 'chromium.android',
      **kwargs
  )


def android_fyi_builder(*, name, **kwargs):
  return ci_builder(
      name = name,
      goma_backend = builders.goma.backend.RBE_PROD,
      mastername = 'chromium.android.fyi',
      **kwargs
  )


def chromium_builder(*, name, **kwargs):
  return ci_builder(
      name = name,
      goma_backend = builders.goma.backend.RBE_PROD,
      mastername = 'chromium',
      **kwargs
  )


def chromiumos_builder(*, name, **kwargs):
  return ci_builder(
      name = name,
      mastername = 'chromium.chromiumos',
      goma_backend = builders.goma.backend.RBE_PROD,
      **kwargs
  )


def clang_builder(*, name, cores=32, properties=None, **kwargs):
  properties = properties or {}
  properties.update({
    'perf_dashboard_machine_group': 'ChromiumClang',
  })
  return ci_builder(
      name = name,
      builderless = True,
      cores = cores,
      # Because these run ToT Clang, goma is not used.
      # Naturally the runtime will be ~4-8h on average, depending on config.
      # CFI builds will take even longer - around 11h.
      execution_timeout = 12 * time.hour,
      mastername = 'chromium.clang',
      properties = properties,
      **kwargs
  )


def clang_mac_builder(*, name, cores=24, **kwargs):
  return clang_builder(
      name = name,
      cores = cores,
      os = builders.os.MAC_10_14,
      ssd = True,
      properties = {
          'xcode_build_version': '11a1027',
      },
      **kwargs
  )


def dawn_builder(*, name, builderless=True, **kwargs):
  return ci.builder(
      name = name,
      builderless = builderless,
      goma_backend = builders.goma.backend.RBE_PROD,
      mastername = 'chromium.dawn',
      service_account =
      'chromium-ci-gpu-builder@chops-service-accounts.iam.gserviceaccount.com',
      **kwargs
  )


def fuzz_builder(*, name, **kwargs):
  return ci.builder(
      name = name,
      goma_backend = builders.goma.backend.RBE_PROD,
      mastername = 'chromium.fuzz',
      notifies = ['chromesec-lkgr-failures'],
      **kwargs
  )


def fuzz_libfuzzer_builder(*, name, **kwargs):
  return fuzz_builder(
      name = name,
      executable = 'recipe:chromium_libfuzzer',
      **kwargs
  )


def fyi_builder(
    *,
    name,
    execution_timeout=10 * time.hour,
    goma_backend=builders.goma.backend.RBE_PROD,
    **kwargs):
  return ci.builder(
      name = name,
      execution_timeout = execution_timeout,
      goma_backend = goma_backend,
      mastername = 'chromium.fyi',
      **kwargs
  )


def fyi_celab_builder(*, name, **kwargs):
  return ci.builder(
      name = name,
      mastername = 'chromium.fyi',
      os = builders.os.WINDOWS_ANY,
      executable = 'recipe:celab',
      goma_backend = builders.goma.backend.RBE_PROD,
      properties = {
          'exclude': 'chrome_only',
          'pool_name': 'celab-chromium-ci',
          'pool_size': 20,
          'tests': '*',
      },
      **kwargs
  )


def fyi_coverage_builder(
    *,
    name,
    cores=32,
    ssd=True,
    execution_timeout=20 * time.hour,
    **kwargs):
  return fyi_builder(
      name = name,
      cores = cores,
      ssd=ssd,
      execution_timeout = execution_timeout,
      **kwargs
  )


def fyi_ios_builder(
    *,
    name,
    caches = None,
    executable='recipe:ios/unified_builder_tester',
    goma_backend=builders.goma.backend.RBE_PROD,
    **kwargs):

  if not caches:
    caches = [builders.xcode_cache.x11c29]

  return fyi_builder(
      name = name,
      caches = caches,
      cores = None,
      executable = executable,
      os = builders.os.MAC_ANY,
      **kwargs
  )


def fyi_mac_builder(
    *,
    name,
    cores=4,
    os=builders.os.MAC_DEFAULT,
    **kwargs):
  return fyi_builder(
      name = name,
      cores = cores,
      goma_backend = builders.goma.backend.RBE_PROD,
      os = os,
      **kwargs
  )


def fyi_windows_builder(
    *,
    name,
    os=builders.os.WINDOWS_DEFAULT,
    **kwargs):
  return fyi_builder(
      name = name,
      os = os,
      **kwargs
  )


def gpu_fyi_builder(*, name, **kwargs):
  return ci.builder(
      name = name,
      mastername = 'chromium.gpu.fyi',
      service_account =
      'chromium-ci-gpu-builder@chops-service-accounts.iam.gserviceaccount.com',
      **kwargs
  )


def gpu_fyi_linux_builder(
    *,
    name,
    execution_timeout=6 * time.hour,
    goma_backend=builders.goma.backend.RBE_PROD,
    **kwargs):
  return gpu_fyi_builder(
      name = name,
      execution_timeout = execution_timeout,
      goma_backend = goma_backend,
      **kwargs
  )


def gpu_fyi_mac_builder(*, name, **kwargs):
  return gpu_fyi_builder(
      name = name,
      cores = 4,
      execution_timeout = 6 * time.hour,
      goma_backend = builders.goma.backend.RBE_PROD,
      os = builders.os.MAC_ANY,
      **kwargs
  )


# Many of the GPU testers are thin testers, they use linux VMS regardless of the
# actual OS that the tests are built for
def gpu_fyi_thin_tester(
    *,
    name,
    execution_timeout=6 * time.hour,
    **kwargs):
  return gpu_fyi_linux_builder(
      name = name,
      cores = 2,
      execution_timeout = execution_timeout,
      # Setting goma_backend for testers is a no-op, but better to be explicit
      # here and also leave the generated configs unchanged for these testers.
      goma_backend = None,
      **kwargs
  )


def gpu_fyi_windows_builder(*, name, **kwargs):
  return gpu_fyi_builder(
      name = name,
      builderless = True,
      goma_backend = builders.goma.backend.RBE_PROD,
      os = builders.os.WINDOWS_ANY,
      **kwargs
  )


def gpu_builder(*, name, **kwargs):
  return ci.builder(
      name = name,
      goma_backend = builders.goma.backend.RBE_PROD,
      mastername = 'chromium.gpu',
      **kwargs
  )


# Many of the GPU testers are thin testers, they use linux VMS regardless of the
# actual OS that the tests are built for
def gpu_thin_tester(*, name, **kwargs):
  return gpu_builder(
      name = name,
      cores = 2,
      os = builders.os.LINUX_DEFAULT,
      **kwargs
  )


def linux_builder(
    *,
    name,
    goma_backend=builders.goma.backend.RBE_PROD,
    goma_jobs=builders.goma.jobs.MANY_JOBS_FOR_CI,
    **kwargs):
  return ci.builder(
      name = name,
      goma_backend = goma_backend,
      goma_jobs = goma_jobs,
      mastername = 'chromium.linux',
      **kwargs
  )


def mac_builder(
    *,
    name,
    cores=None,
    goma_backend=builders.goma.backend.RBE_PROD,
    os=builders.os.MAC_DEFAULT,
    **kwargs):
  return ci.builder(
      name = name,
      cores = cores,
      goma_backend = goma_backend,
      mastername = 'chromium.mac',
      os = os,
      **kwargs
  )


def mac_ios_builder(*,
                    name,
                    caches=None,
                    executable='recipe:ios/unified_builder_tester',
                    goma_backend=builders.goma.backend.RBE_PROD,
                    properties=None,
                    **kwargs):
  if not caches:
    caches = [builders.xcode_cache.x11c29]
  if not properties:
    properties = {
      'xcode_build_version': '11c29'
    }

  return mac_builder(
      name = name,
      caches = caches,
      goma_backend = goma_backend,
      executable = executable,
      os = builders.os.MAC_ANY,
      properties = properties,
      **kwargs
  )


def memory_builder(
    *,
    name,
    goma_jobs=builders.goma.jobs.MANY_JOBS_FOR_CI,
    **kwargs):
  return ci.builder(
      name = name,
      goma_backend = builders.goma.backend.RBE_PROD,
      goma_jobs = goma_jobs,
      mastername = 'chromium.memory',
      **kwargs
  )


def swangle_builder(*, name, **kwargs):
  return ci.builder(
      name = name,
      builderless = True,
      mastername = 'chromium.swangle',
      service_account =
      'chromium-ci-gpu-builder@chops-service-accounts.iam.gserviceaccount.com',
      **kwargs
  )


def swangle_linux_builder(
    *,
    name,
    **kwargs):
  return swangle_builder(
      name = name,
      goma_backend = builders.goma.backend.RBE_PROD,
      os = builders.os.LINUX_DEFAULT,
      **kwargs
  )


def swangle_windows_builder(*, name, **kwargs):
  return swangle_builder(
      name = name,
      goma_backend = builders.goma.backend.RBE_PROD,
      os = builders.os.WINDOWS_DEFAULT,
      **kwargs
  )


def win_builder(*, name, os=builders.os.WINDOWS_DEFAULT, **kwargs):
  return ci.builder(
      name = name,
      goma_backend = builders.goma.backend.RBE_PROD,
      mastername = 'chromium.win',
      os = os,
      **kwargs
  )


ci = struct(
    builder = ci_builder,
    defaults = defaults,

    android_builder = android_builder,
    android_fyi_builder = android_fyi_builder,
    chromium_builder = chromium_builder,
    chromiumos_builder = chromiumos_builder,
    clang_builder = clang_builder,
    clang_mac_builder = clang_mac_builder,
    dawn_builder = dawn_builder,
    fuzz_builder = fuzz_builder,
    fuzz_libfuzzer_builder = fuzz_libfuzzer_builder,
    fyi_builder = fyi_builder,
    fyi_celab_builder = fyi_celab_builder,
    fyi_coverage_builder = fyi_coverage_builder,
    fyi_ios_builder = fyi_ios_builder,
    fyi_mac_builder = fyi_mac_builder,
    fyi_windows_builder = fyi_windows_builder,
    gpu_fyi_linux_builder = gpu_fyi_linux_builder,
    gpu_fyi_mac_builder = gpu_fyi_mac_builder,
    gpu_fyi_thin_tester = gpu_fyi_thin_tester,
    gpu_fyi_windows_builder = gpu_fyi_windows_builder,
    gpu_builder = gpu_builder,
    gpu_thin_tester = gpu_thin_tester,
    linux_builder = linux_builder,
    mac_builder = mac_builder,
    mac_ios_builder = mac_ios_builder,
    memory_builder = memory_builder,
    swangle_linux_builder = swangle_linux_builder,
    swangle_windows_builder = swangle_windows_builder,
    win_builder = win_builder,
)
