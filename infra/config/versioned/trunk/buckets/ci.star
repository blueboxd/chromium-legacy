load('//lib/builders.star', 'builder_name', 'cpu', 'goma', 'os')
load('//lib/ci.star', 'ci')
# Load this using relative path so that the load statement doesn't
# need to be changed when making a new milestone
load('../vars.star', 'vars')

luci.bucket(
    name = vars.ci_bucket,
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

luci.gitiles_poller(
    name = vars.ci_poller,
    bucket = vars.ci_bucket,
    repo = 'https://chromium.googlesource.com/chromium/src',
    refs = [vars.ref],
)


ci.defaults.bucket.set(vars.ci_bucket)
ci.defaults.bucketed_triggers.set(True)
ci.defaults.triggered_by.set([vars.ci_poller])


# Builders are sorted first lexicographically by the function used to define
# them, then lexicographically by their name


ci.android_builder(
    name = 'Android WebView M (dbg)',
    triggered_by = [builder_name('Android arm64 Builder (dbg)')],
)

ci.android_builder(
    name = 'Android WebView N (dbg)',
    triggered_by = [builder_name('Android arm64 Builder (dbg)')],
)

ci.android_builder(
    name = 'Android WebView O (dbg)',
    triggered_by = [builder_name('Android arm64 Builder (dbg)')],
)

ci.android_builder(
    name = 'Android WebView P (dbg)',
    triggered_by = [builder_name('Android arm64 Builder (dbg)')],
)

ci.android_builder(
    name = 'Android arm Builder (dbg)',
    execution_timeout = 4 * time.hour,
)

ci.android_builder(
    name = 'Android arm64 Builder (dbg)',
    goma_jobs = goma.jobs.MANY_JOBS_FOR_CI,
    execution_timeout = 4 * time.hour,
)

ci.android_builder(
    name = 'Android x64 Builder (dbg)',
    execution_timeout = 4 * time.hour,
)

ci.android_builder(
    name = 'Android x86 Builder (dbg)',
)

ci.android_builder(
    name = 'Cast Android (dbg)',
)

ci.android_builder(
    name = 'Marshmallow 64 bit Tester',
    triggered_by = [builder_name('Android arm64 Builder (dbg)')],
)

ci.android_builder(
    name = 'Nougat Phone Tester',
    triggered_by = [builder_name('Android arm64 Builder (dbg)')],
)

ci.android_builder(
    name = 'Oreo Phone Tester',
    triggered_by = [builder_name('Android arm64 Builder (dbg)')],
)

ci.android_builder(
    name = 'android-cronet-arm-dbg',
    notifies = ['cronet'],
)

ci.android_builder(
    name = 'android-cronet-arm-rel',
    notifies = ['cronet'],
)

ci.android_builder(
    name = 'android-cronet-kitkat-arm-rel',
    notifies = ['cronet'],
    triggered_by = [builder_name('android-cronet-arm-rel')],
)

ci.android_builder(
    name = 'android-cronet-lollipop-arm-rel',
    notifies = ['cronet'],
    triggered_by = [builder_name('android-cronet-arm-rel')],
)

ci.android_builder(
    name = 'android-kitkat-arm-rel',
)

ci.android_builder(
    name = 'android-marshmallow-arm64-rel',
)

ci.android_builder(
    name = 'android-pie-arm64-dbg',
    triggered_by = [builder_name('Android arm64 Builder (dbg)')],
)

ci.android_builder(
    name = 'android-pie-arm64-rel',
)


ci.chromiumos_builder(
    name = 'chromeos-amd64-generic-dbg',
)

ci.chromiumos_builder(
    name = 'chromeos-amd64-generic-rel',
)

ci.chromiumos_builder(
    name = 'chromeos-arm-generic-rel',
)

ci.chromiumos_builder(
    name = 'chromeos-kevin-rel',
)

ci.fyi_builder(
    name = 'chromeos-kevin-rel-hw-tests',
)

ci.chromiumos_builder(
    name = 'linux-chromeos-dbg',
)

ci.chromiumos_builder(
    name = 'linux-chromeos-rel',
)


ci.dawn_builder(
    name = 'Dawn Linux x64 DEPS Builder',
)

ci.dawn_builder(
    name = 'Dawn Linux x64 DEPS Release (Intel HD 630)',
    cores = 2,
    os = os.LINUX_DEFAULT,
    triggered_by = [builder_name('Dawn Linux x64 DEPS Builder')],
)

ci.dawn_builder(
    name = 'Dawn Linux x64 DEPS Release (NVIDIA)',
    cores = 2,
    os = os.LINUX_DEFAULT,
    triggered_by = [builder_name('Dawn Linux x64 DEPS Builder')],
)

ci.dawn_builder(
    name = 'Dawn Mac x64 DEPS Builder',
    builderless = False,
    cores = None,
    os = os.MAC_ANY,
)

# Note that the Mac testers are all thin Linux VMs, triggering jobs on the
# physical Mac hardware in the Swarming pool which is why they run on linux
ci.dawn_builder(
    name = 'Dawn Mac x64 DEPS Release (AMD)',
    cores = 2,
    os = os.LINUX_DEFAULT,
    triggered_by = [builder_name('Dawn Mac x64 DEPS Builder')],
)

ci.dawn_builder(
    name = 'Dawn Mac x64 DEPS Release (Intel)',
    cores = 2,
    os = os.LINUX_DEFAULT,
    triggered_by = [builder_name('Dawn Mac x64 DEPS Builder')],
)

ci.dawn_builder(
    name = 'Dawn Win10 x64 DEPS Builder',
    os = os.WINDOWS_ANY,
)

ci.dawn_builder(
    name = 'Dawn Win10 x64 DEPS Release (Intel HD 630)',
    cores = 2,
    os = os.LINUX_DEFAULT,
    triggered_by = [builder_name('Dawn Win10 x64 DEPS Builder')],
)

ci.dawn_builder(
    name = 'Dawn Win10 x64 DEPS Release (NVIDIA)',
    cores = 2,
    os = os.LINUX_DEFAULT,
    triggered_by = [builder_name('Dawn Win10 x64 DEPS Builder')],
)

ci.dawn_builder(
    name = 'Dawn Win10 x86 DEPS Builder',
    os = os.WINDOWS_ANY,
)

ci.dawn_builder(
    name = 'Dawn Win10 x86 DEPS Release (Intel HD 630)',
    cores = 2,
    os = os.LINUX_DEFAULT,
    triggered_by = [builder_name('Dawn Win10 x86 DEPS Builder')],
)

ci.dawn_builder(
    name = 'Dawn Win10 x86 DEPS Release (NVIDIA)',
    cores = 2,
    os = os.LINUX_DEFAULT,
    triggered_by = [builder_name('Dawn Win10 x86 DEPS Builder')],
)


ci.fyi_builder(
    name = 'VR Linux',
)

# This is launching & collecting entirely isolated tests.
# OS shouldn't matter.
ci.fyi_builder(
    name = 'mac-osxbeta-rel',
    goma_backend = None,
    triggered_by = [builder_name('Mac Builder')],
)


ci.fyi_windows_builder(
    name = 'Win10 Tests x64 1803',
    goma_backend = None,
    os = os.WINDOWS_10,
    triggered_by = [builder_name('Win x64 Builder')],
)


ci.gpu_builder(
    name = 'Android Release (Nexus 5X)',
)

ci.gpu_builder(
    name = 'GPU Linux Builder',
)

ci.gpu_builder(
    name = 'GPU Mac Builder',
    cores = None,
    os = os.MAC_ANY,
)

ci.gpu_builder(
    name = 'GPU Win x64 Builder',
    builderless = True,
    os = os.WINDOWS_ANY,
)


ci.gpu_thin_tester(
    name = 'Linux Release (NVIDIA)',
    triggered_by = [builder_name('GPU Linux Builder')],
)

ci.gpu_thin_tester(
    name = 'Mac Release (Intel)',
    triggered_by = [builder_name('GPU Mac Builder')],
)

ci.gpu_thin_tester(
    name = 'Mac Retina Release (AMD)',
    triggered_by = [builder_name('GPU Mac Builder')],
)

ci.gpu_thin_tester(
    name = 'Win10 x64 Release (NVIDIA)',
    triggered_by = [builder_name('GPU Win x64 Builder')],
)


ci.linux_builder(
    name = 'Cast Linux',
    goma_jobs = goma.jobs.J50,
)

ci.linux_builder(
    name = 'Fuchsia ARM64',
    notifies = ['cr-fuchsia'],
)

ci.linux_builder(
    name = 'Fuchsia x64',
    notifies = ['cr-fuchsia'],
)

ci.linux_builder(
    name = 'Linux Builder',
)

ci.linux_builder(
    name = 'Linux Builder (dbg)',
)

ci.linux_builder(
    name = 'Linux Tests',
    goma_backend = None,
    triggered_by = [builder_name('Linux Builder')],
)

ci.linux_builder(
    name = 'Linux Tests (dbg)(1)',
    triggered_by = [builder_name('Linux Builder (dbg)')],
)

ci.linux_builder(
    name = 'fuchsia-arm64-cast',
    notifies = ['cr-fuchsia'],
)

ci.linux_builder(
    name = 'fuchsia-x64-cast',
    notifies = ['cr-fuchsia'],
)

ci.linux_builder(
    name = 'linux-ozone-rel',
)

ci.linux_builder(
    name = 'Linux Ozone Tester (Headless)',
    triggered_by = [builder_name('linux-ozone-rel')],
)

ci.linux_builder(
    name = 'Linux Ozone Tester (Wayland)',
    triggered_by = [builder_name('linux-ozone-rel')],
)

ci.linux_builder(
    name = 'Linux Ozone Tester (X11)',
    triggered_by = [builder_name('linux-ozone-rel')],
)


ci.mac_builder(
    name = 'Mac Builder',
    os = os.MAC_10_14,
)

ci.mac_builder(
    name = 'Mac Builder (dbg)',
    os = os.MAC_ANY,
)

# The build runs on 10.13, but triggers tests on 10.10 bots.
ci.mac_builder(
    name = 'Mac10.10 Tests',
    triggered_by = [builder_name('Mac Builder')],
)

# The build runs on 10.13, but triggers tests on 10.11 bots.
ci.mac_builder(
    name = 'Mac10.11 Tests',
    triggered_by = [builder_name('Mac Builder')],
)

ci.mac_builder(
    name = 'Mac10.12 Tests',
    os = os.MAC_10_12,
    triggered_by = [builder_name('Mac Builder')],
)

ci.mac_builder(
    name = 'Mac10.13 Tests',
    os = os.MAC_10_13,
    triggered_by = [builder_name('Mac Builder')],
)

ci.mac_builder(
    name = 'Mac10.14 Tests',
    os = os.MAC_10_14,
    triggered_by = [builder_name('Mac Builder')],
)

ci.mac_builder(
    name = 'Mac10.13 Tests (dbg)',
    os = os.MAC_ANY,
    triggered_by = [builder_name('Mac Builder (dbg)')],
)

ci.mac_builder(
    name = 'WebKit Mac10.13 (retina)',
    os = os.MAC_10_13,
    triggered_by = [builder_name('Mac Builder')],
)


ci.mac_ios_builder(
    name = 'ios-simulator',
    executable = 'recipe:chromium',
    properties = {
        'xcode_build_version': '11c29',
    },
)


ci.memory_builder(
    name = 'Linux ASan LSan Builder',
    ssd = True,
)

ci.memory_builder(
    name = 'Linux ASan LSan Tests (1)',
    triggered_by = [builder_name('Linux ASan LSan Builder')],
)

ci.memory_builder(
    name = 'Linux ASan Tests (sandboxed)',
    triggered_by = [builder_name('Linux ASan LSan Builder')],
)


ci.win_builder(
    name = 'Win7 Tests (dbg)(1)',
    os = os.WINDOWS_7,
    triggered_by = [builder_name('Win Builder (dbg)')],
)

ci.win_builder(
    name = 'Win 7 Tests x64 (1)',
    os = os.WINDOWS_7,
    triggered_by = [builder_name('Win x64 Builder')],
)

ci.win_builder(
    name = 'Win Builder (dbg)',
    cores = 32,
    os = os.WINDOWS_ANY,
)

ci.win_builder(
    name = 'Win x64 Builder',
    cores = 32,
    os = os.WINDOWS_ANY,
)

ci.win_builder(
    name = 'Win10 Tests x64',
    triggered_by = [builder_name('Win x64 Builder')],
)
