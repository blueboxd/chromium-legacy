load('//lib/builders.star', 'builder', 'cpu', 'defaults', 'goma', 'os')
load('//lib/try.star', 'try_')

try_.defaults.bucket.set('try')
try_.defaults.build_numbers.set(True)
try_.defaults.configure_kitchen.set(True)
try_.defaults.cores.set(8)
try_.defaults.cpu.set(cpu.X86_64)
try_.defaults.cq_group.set('cq')
try_.defaults.executable.set('recipe:chromium_trybot')
try_.defaults.execution_timeout.set(6 * time.hour)
try_.defaults.expiration_timeout.set(2 * time.hour)
try_.defaults.os.set(os.LINUX_DEFAULT)
try_.defaults.service_account.set('chromium-try-gpu-builder@chops-service-accounts.iam.gserviceaccount.com')
try_.defaults.swarming_tags.set(['vpython:native-python-wrapper'])
try_.defaults.task_template_canary_percentage.set(5)

try_.defaults.caches.set([
    swarming.cache(
        name = 'win_toolchain',
        path = 'win_toolchain',
    ),
])


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

def gpu_android_builder(*, name, **kwargs):
  return try_.builder(
      name = name,
      builderless = True,
      goma_backend = goma.backend.RBE_PROD,
      mastername = 'tryserver.chromium.android',
      ssd = None,
      **kwargs
  )

gpu_android_builder(
    name = 'gpu-fyi-try-android-l-nexus-5-32',
    pool = 'luci.chromium.gpu.android.nexus5.try',
)

gpu_android_builder(
    name = 'gpu-fyi-try-android-l-nexus-6-32',
    pool = 'luci.chromium.gpu.android.nexus6.try',
)

gpu_android_builder(
    name = 'gpu-fyi-try-android-m-nexus-5x-64',
    pool = 'luci.chromium.gpu.android.nexus5x.try',
)

gpu_android_builder(
    name = 'gpu-fyi-try-android-m-nexus-5x-deqp-64',
    pool = 'luci.chromium.gpu.android.nexus5x.try',
)

gpu_android_builder(
    name = 'gpu-fyi-try-android-m-nexus-5x-skgl-64',
    pool = 'luci.chromium.gpu.android.nexus5x.try',
)

gpu_android_builder(
    name = 'gpu-fyi-try-android-m-nexus-6p-64',
    pool = 'luci.chromium.gpu.android.nexus6p.try',
)

gpu_android_builder(
    name = 'gpu-fyi-try-android-m-nexus-9-64',
    pool = 'luci.chromium.gpu.android.nexus9.try',
)

gpu_android_builder(
    name = 'gpu-fyi-try-android-n-nvidia-shield-tv-64',
    pool = 'luci.chromium.gpu.android.nvidia.shield.tv.try',
)

gpu_android_builder(
    name = 'gpu-fyi-try-android-p-pixel-2-32',
    pool = 'luci.chromium.gpu.android.pixel2.chromium.try',
)

gpu_android_builder(
    name = 'gpu-fyi-try-android-p-pixel-2-skv-32',
    pool = 'luci.chromium.gpu.android.pixel2.chromium.try',
)

gpu_android_builder(
    name = 'gpu-fyi-try-android-q-pixel-2-deqp-vk-32',
    pool = 'luci.chromium.gpu.android.pixel2.try',
)

gpu_android_builder(
    name = 'gpu-fyi-try-android-q-pixel-2-deqp-vk-64',
    pool = 'luci.chromium.gpu.android.pixel2.try',
)

gpu_android_builder(
    name = 'gpu-fyi-try-android-q-pixel-2-vk-32',
    pool = 'luci.chromium.gpu.android.pixel2.try',
)

gpu_android_builder(
    name = 'gpu-fyi-try-android-q-pixel-2-vk-64',
    pool = 'luci.chromium.gpu.android.pixel2.try',
)

gpu_android_builder(
    name = 'gpu-try-android-m-nexus-5x-64',
    pool = 'luci.chromium.gpu.android.nexus5x.try',
)


def gpu_linux_builder(*, name, **kwargs):
  return try_.builder(
      name = name,
      builderless = True,
      goma_backend = goma.backend.RBE_PROD,
      mastername = 'tryserver.chromium.linux',
      ssd = None,
      **kwargs
  )

gpu_linux_builder(
    name = 'gpu-fyi-try-linux-intel-dqp',
    pool = 'luci.chromium.gpu.linux.intel.try',
)

gpu_linux_builder(
    name = 'gpu-fyi-try-linux-intel-exp',
    pool = 'luci.chromium.gpu.linux.intel.try',
)

gpu_linux_builder(
    name = 'gpu-fyi-try-linux-intel-rel',
    pool = 'luci.chromium.gpu.linux.intel.try',
)

gpu_linux_builder(
    name = 'gpu-fyi-try-linux-intel-sk-dawn-rel',
    pool = 'luci.chromium.gpu.linux.intel.try',
)

gpu_linux_builder(
    name = 'gpu-fyi-try-linux-intel-skv',
    pool = 'luci.chromium.gpu.linux.intel.try',
)

gpu_linux_builder(
    name = 'gpu-fyi-try-linux-nvidia-dbg',
    pool = 'luci.chromium.gpu.linux.nvidia.try',
)

gpu_linux_builder(
    name = 'gpu-fyi-try-linux-nvidia-dqp',
    pool = 'luci.chromium.gpu.linux.nvidia.try',
)

gpu_linux_builder(
    name = 'gpu-fyi-try-linux-nvidia-exp',
    pool = 'luci.chromium.gpu.linux.nvidia.try',
)

gpu_linux_builder(
    name = 'gpu-fyi-try-linux-nvidia-rel',
    pool = 'luci.chromium.gpu.linux.nvidia.try',
)

gpu_linux_builder(
    name = 'gpu-fyi-try-linux-nvidia-skv',
    pool = 'luci.chromium.gpu.linux.nvidia.try',
)

gpu_linux_builder(
    name = 'gpu-fyi-try-linux-nvidia-tsn',
    pool = 'luci.chromium.gpu.linux.nvidia.try',
)

gpu_linux_builder(
    name = 'gpu-try-linux-nvidia-dbg',
    pool = 'luci.chromium.gpu.linux.nvidia.try',
)

gpu_linux_builder(
    name = 'gpu-try-linux-nvidia-rel',
    pool = 'luci.chromium.gpu.linux.nvidia.try',
)


def gpu_mac_builder(*, name, **kwargs):
  return try_.builder(
      name = name,
      builderless = True,
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


def gpu_win_builder(*, name, **kwargs):
  return try_.builder(
      name = name,
      builderless = True,
      goma_backend = goma.backend.RBE_PROD,
      mastername = 'tryserver.chromium.win',
      os = os.WINDOWS_ANY,
      ssd = None,
      **kwargs
  )

gpu_win_builder(
    name = 'gpu-fyi-try-win10-intel-dqp-64',
    pool = 'luci.chromium.gpu.win10.intel.try',
)

gpu_win_builder(
    name = 'gpu-fyi-try-win10-intel-exp-64',
    pool = 'luci.chromium.gpu.win10.intel.try',
)

gpu_win_builder(
    name = 'gpu-fyi-try-win10-intel-rel-64',
    pool = 'luci.chromium.gpu.win10.intel.try',
)

gpu_win_builder(
    name = 'gpu-fyi-try-win10-nvidia-dbg-64',
    pool = 'luci.chromium.gpu.win10.nvidia.try',
)

gpu_win_builder(
    name = 'gpu-fyi-try-win10-nvidia-dqp-64',
    pool = 'luci.chromium.gpu.win10.nvidia.try',
)

gpu_win_builder(
    name = 'gpu-fyi-try-win10-nvidia-dx12vk-dbg-64',
    pool = 'luci.chromium.gpu.win10.nvidia.try',
)

gpu_win_builder(
    name = 'gpu-fyi-try-win10-nvidia-dx12vk-rel-64',
    pool = 'luci.chromium.gpu.win10.nvidia.try',
)

gpu_win_builder(
    name = 'gpu-fyi-try-win10-nvidia-exp-64',
    pool = 'luci.chromium.gpu.win10.nvidia.try',
)

gpu_win_builder(
    name = 'gpu-fyi-try-win10-nvidia-sk-dawn-rel-64',
    pool = 'luci.chromium.gpu.win10.nvidia.try',
)

gpu_win_builder(
    name = 'gpu-fyi-try-win10-nvidia-skgl-64',
    pool = 'luci.chromium.gpu.win10.nvidia.try',
)

gpu_win_builder(
    name = 'gpu-fyi-try-win10-nvidia-rel-32',
    pool = 'luci.chromium.gpu.win10.nvidia.try',
)

gpu_win_builder(
    name = 'gpu-fyi-try-win10-nvidia-rel-64',
    pool = 'luci.chromium.gpu.win10.nvidia.try',
)

gpu_win_builder(
    name = 'gpu-fyi-try-win7-amd-dbg-32',
    pool = 'luci.chromium.gpu.win7.amd.try',
)

gpu_win_builder(
    name = 'gpu-fyi-try-win7-amd-dqp-32',
    pool = 'luci.chromium.gpu.win7.amd.try',
)

gpu_win_builder(
    name = 'gpu-fyi-try-win7-amd-rel-32',
    pool = 'luci.chromium.gpu.win7.amd.try',
)

gpu_win_builder(
    name = 'gpu-fyi-try-win7-nvidia-dqp-64',
    pool = 'luci.chromium.gpu.win7.nvidia.try',
)

gpu_win_builder(
    name = 'gpu-fyi-try-win7-nvidia-rel-32',
    pool = 'luci.chromium.gpu.win7.nvidia.try',
)

gpu_win_builder(
    name = 'gpu-fyi-try-win7-nvidia-rel-64',
    pool = 'luci.chromium.gpu.win7.nvidia.try',
)

gpu_win_builder(
    name = 'gpu-try-win10-nvidia-rel',
    pool = 'luci.chromium.gpu.win10.nvidia.try',
)
