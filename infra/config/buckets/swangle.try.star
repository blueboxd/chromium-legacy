load('//lib/builders.star', 'builder', 'cpu', 'defaults', 'goma', 'os')
load('//lib/try.star', 'try_')

try_.defaults.bucket.set('try')
try_.defaults.build_numbers.set(True)
try_.defaults.configure_kitchen.set(True)
try_.defaults.cores.set(8)
try_.defaults.cpu.set(cpu.X86_64)
try_.defaults.cq_group.set('cq')
try_.defaults.executable.set('recipe:chromium_trybot')
try_.defaults.execution_timeout.set(2 * time.hour)
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


try_.chromium_swangle_linux_builder(
    name = 'linux-swangle-try-tot-angle-x64',
    pool = 'luci.chromium.swangle.angle.linux.x64.try',
)

try_.chromium_swangle_linux_builder(
    name = 'linux-swangle-try-tot-angle-x86',
    pool = 'luci.chromium.swangle.linux.x86.try',
)

try_.chromium_swangle_linux_builder(
    name = 'linux-swangle-try-tot-swiftshader-x64',
    pool = 'luci.chromium.swangle.sws.linux.x64.try',
)

try_.chromium_swangle_linux_builder(
    name = 'linux-swangle-try-tot-swiftshader-x86',
    pool = 'luci.chromium.swangle.linux.x86.try',
)

try_.chromium_swangle_linux_builder(
    name = 'linux-swangle-try-x64',
    pool = 'luci.chromium.swangle.chromium.linux.x64.try',
)

try_.chromium_swangle_linux_builder(
    name = 'linux-swangle-try-x86',
    pool = 'luci.chromium.swangle.linux.x86.try',
)


try_.chromium_swangle_windows_builder(
    name = 'win-swangle-try-tot-angle-x64',
    pool = 'luci.chromium.swangle.win.x64.try',
)

try_.chromium_swangle_windows_builder(
    name = 'win-swangle-try-tot-angle-x86',
    pool = 'luci.chromium.swangle.angle.win.x86.try',
)

try_.chromium_swangle_windows_builder(
    name = 'win-swangle-try-tot-swiftshader-x64',
    pool = 'luci.chromium.swangle.win.x64.try',
)

try_.chromium_swangle_windows_builder(
    name = 'win-swangle-try-tot-swiftshader-x86',
    pool = 'luci.chromium.swangle.sws.win.x86.try',
)

try_.chromium_swangle_windows_builder(
    name = 'win-swangle-try-x64',
    pool = 'luci.chromium.swangle.win.x64.try',
)

try_.chromium_swangle_windows_builder(
    name = 'win-swangle-try-x86',
    pool = 'luci.chromium.swangle.chromium.win.x86.try',
)

