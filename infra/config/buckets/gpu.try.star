load('//lib/builders.star', 'builder', 'cpu', 'defaults', 'goma', 'os')

luci.recipe.defaults.cipd_package.set('infra/recipe_bundles/chromium.googlesource.com/chromium/tools/build')

defaults.bucket.set('try')
defaults.build_numbers.set(True)
defaults.configure_kitchen.set(True)
defaults.cores.set(8)
defaults.cpu.set(cpu.X86_64)
defaults.executable.set(luci.recipe(name = 'chromium_trybot'))
defaults.execution_timeout.set(6 * time.hour)
defaults.expiration_timeout.set(2 * time.hour)
defaults.os.set(os.LINUX_DEFAULT)
defaults.service_account.set('chromium-try-gpu-builder@chops-service-accounts.iam.gserviceaccount.com')
defaults.swarming_tags.set(['vpython:native-python-wrapper'])
defaults.task_template_canary_percentage.set(5)

defaults.caches.set([
    swarming.cache(
        name = 'win_toolchain',
        path = 'win_toolchain',
    ),
])


def tryjob(
    *,
    disable_reuse=None,
    experiment_percentage=None,
    location_regexp=None,
    location_regexp_exclude=None):
  return struct(
      disable_reuse = disable_reuse,
      experiment_percentage = experiment_percentage,
      location_regexp = location_regexp,
      location_regexp_exclude = location_regexp_exclude,
  )

def try_builder(
    *,
    name,
    tryjob=None,
    **kwargs):
  if tryjob != None:
    luci.cq_tryjob_verifier(
        builder = 'try/' + name,
        cq_group = 'cq',
        disable_reuse = tryjob.disable_reuse,
        experiment_percentage = tryjob.experiment_percentage,
        location_regexp = tryjob.location_regexp,
        location_regexp_exclude = tryjob.location_regexp_exclude,
    )

  return builder(
      name = name,
      **kwargs
  )


# Builders appear after the function used to define them, with all builders
# defined using the same function ordered lexicographically by name
# Builder functions are defined in lexicographic order by name ignoring the
# '_builder' suffix

# Builder functions are defined for GPU builders on each master where they
# appear: gpu_XXX_builder where XXX is the part after the last dot in the
# mastername
# Builder functions are defined for each master, with additional functions
# for specializing on OS: XXX_builder and XXX_YYY_builder where XXX is the part
# after the last dot in the mastername and YYY is the OS


def gpu_mac_builder(*, name, builderless=True, **kwargs):
  return try_builder(
      name = name,
      builderless = builderless,
      cores = None,
      goma_backend = goma.backend.RBE_PROD,
      mastername = 'tryserver.chromium.mac',
      os = os.MAC_ANY,
      ssd = None,
      **kwargs
  )

gpu_mac_builder(
    name = 'gpu-fyi-try-mac-amd-dqp',
    pool = 'luci.chromium.gpu.mac.retina.amd.try',
)

gpu_mac_builder(
    name = 'gpu-fyi-try-mac-amd-pro-rel',
    pool = 'luci.chromium.gpu.mac.pro.amd.try',
)

gpu_mac_builder(
    name = 'gpu-fyi-try-mac-amd-retina-dbg',
    pool = 'luci.chromium.gpu.mac.retina.amd.try',
)

gpu_mac_builder(
    name = 'gpu-fyi-try-mac-amd-retina-exp',
    pool = 'luci.chromium.gpu.mac.retina.amd.try',
)

gpu_mac_builder(
    name = 'gpu-fyi-try-mac-amd-retina-rel',
    pool = 'luci.chromium.gpu.mac.retina.amd.try',
)

gpu_mac_builder(
    name = 'gpu-fyi-try-mac-asan',
    # This bot actually uses both Mac Retina AMD and Mac Mini Intel resources.
    # Group it in Mac Retina AMD users pool, since it is smaller.
    pool = 'luci.chromium.gpu.mac.retina.amd.try',
)

gpu_mac_builder(
    name = 'gpu-fyi-try-mac-intel-dbg',
    pool = 'luci.chromium.gpu.mac.mini.intel.try',
)

gpu_mac_builder(
    name = 'gpu-fyi-try-mac-intel-dqp',
    pool = 'luci.chromium.gpu.mac.mini.intel.try',
)

gpu_mac_builder(
    name = 'gpu-fyi-try-mac-intel-exp',
    pool = 'luci.chromium.gpu.mac.mini.intel.try',
)

gpu_mac_builder(
    name = 'gpu-fyi-try-mac-intel-rel',
    pool = 'luci.chromium.gpu.mac.mini.intel.try',
)

gpu_mac_builder(
    name = 'gpu-fyi-try-mac-nvidia-retina-dbg',
    pool = 'luci.chromium.gpu.mac.retina.nvidia.try',
)

gpu_mac_builder(
    name = 'gpu-fyi-try-mac-nvidia-retina-exp',
    # This bot has one machine backing its tests at the moment.
    # If it gets more, the modified execution_timeout should be removed.
    # See crbug.com/853307 for more context.
    execution_timeout = 12 * time.hour,
    pool = 'luci.chromium.gpu.mac.retina.nvidia.try',
)

gpu_mac_builder(
    name = 'gpu-fyi-try-mac-nvidia-retina-rel',
    pool = 'luci.chromium.gpu.mac.retina.nvidia.try',
)

gpu_mac_builder(
    name = 'gpu-try-mac-amd-retina-dbg',
    pool = 'luci.chromium.gpu.mac.retina.amd.try',
)

gpu_mac_builder(
    name = 'gpu-try-mac-intel-dbg',
    pool = 'luci.chromium.gpu.mac.mini.intel.try',
)

