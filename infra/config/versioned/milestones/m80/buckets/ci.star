load('//lib/builders.star', 'builder_name', 'cpu', 'defaults', 'goma', 'os')
load('//lib/ci.star', 'ci')
# Load this using relative path so that the load statement doesn't
# need to be changed when making a new milestone
load('../vars.star', 'vars')

defaults.pool.set('luci.chromium.ci')

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

ci.main_console_view(
    name = vars.main_console_name,
    header = '//consoles/chromium-header.textpb',
    repo = 'https://chromium.googlesource.com/chromium/src',
    refs = [vars.ref],
    title = vars.main_console_title,
    top_level_ordering = [
        'chromium',
        'chromium.win',
        'chromium.mac',
        'chromium.linux',
        'chromium.chromiumos',
        'chromium.android',
        'chrome',
        'chromium.memory',
        'chromium.dawn',
        'chromium.gpu',
        'chromium.fyi',
        'chromium.android.fyi',
        'chromium.clang',
        'chromium.fuzz',
        'chromium.gpu.fyi',
        'chromium.swangle',
    ],
)


ci.defaults.add_to_console_view.set(vars.is_master)
ci.defaults.bucket.set(vars.ci_bucket)
ci.defaults.bucketed_triggers.set(True)
ci.defaults.main_console_view.set(vars.main_console_name)
ci.defaults.triggered_by.set([vars.ci_poller])


# Builders are sorted first lexicographically by the function used to define
# them, then lexicographically by their name


ci.android_builder(
    name = 'android-kitkat-arm-rel',
    console_view_entry = ci.console_view_entry(
        category = 'on_cq',
        short_name = 'K',
    ),
)

ci.android_builder(
    name = 'android-marshmallow-arm64-rel',
    console_view_entry = ci.console_view_entry(
        category = 'on_cq',
        short_name = 'M',
    ),
)


ci.chromiumos_builder(
    name = 'chromeos-amd64-generic-rel',
    console_view_entry = ci.console_view_entry(
        category = 'simple|release|x64',
        short_name = 'rel',
    ),
)

ci.chromiumos_builder(
    name = 'linux-chromeos-rel',
    console_view_entry = ci.console_view_entry(
        category = 'default',
        short_name = 'rel',
    ),
)


# This is launching & collecting entirely isolated tests.
# OS shouldn't matter.
ci.fyi_builder(
    name = 'mac-osxbeta-rel',
    console_view_entry = ci.console_view_entry(
        category = 'mac',
        short_name = 'beta',
    ),
    goma_backend = None,
    triggered_by = [builder_name('Mac Builder')],
)


ci.fyi_windows_builder(
    name = 'Win10 Tests x64 1803',
    console_view_entry = ci.console_view_entry(
        category = 'win10|1803',
    ),
    goma_backend = None,
    os = os.WINDOWS_10,
    triggered_by = [builder_name('Win x64 Builder')],
)


ci.gpu_builder(
    name = 'Android Release (Nexus 5X)',
    console_view_entry = ci.console_view_entry(
        category = 'Android',
    ),
)

ci.gpu_builder(
    name = 'GPU Linux Builder',
    console_view_entry = ci.console_view_entry(
        category = 'Linux',
    ),
)

ci.gpu_builder(
    name = 'GPU Mac Builder',
    console_view_entry = ci.console_view_entry(
        category = 'Mac',
    ),
    cores = None,
    os = os.MAC_ANY,
)

ci.gpu_builder(
    name = 'GPU Win x64 Builder',
    builderless = True,
    console_view_entry = ci.console_view_entry(
        category = 'Windows',
    ),
    os = os.WINDOWS_ANY,
)


ci.gpu_thin_tester(
    name = 'Linux Release (NVIDIA)',
    console_view_entry = ci.console_view_entry(
        category = 'Linux',
    ),
    triggered_by = [builder_name('GPU Linux Builder')],
)

ci.gpu_thin_tester(
    name = 'Mac Release (Intel)',
    console_view_entry = ci.console_view_entry(
        category = 'Mac',
    ),
    triggered_by = [builder_name('GPU Mac Builder')],
)

ci.gpu_thin_tester(
    name = 'Mac Retina Release (AMD)',
    console_view_entry = ci.console_view_entry(
        category = 'Mac',
    ),
    triggered_by = [builder_name('GPU Mac Builder')],
)

ci.gpu_thin_tester(
    name = 'Win10 x64 Release (NVIDIA)',
    console_view_entry = ci.console_view_entry(
        category = 'Windows',
    ),
    triggered_by = [builder_name('GPU Win x64 Builder')],
)


ci.linux_builder(
    name = 'Linux Builder',
    console_view_entry = ci.console_view_entry(
        category = 'release',
        short_name = 'bld',
    ),
)

ci.linux_builder(
    name = 'Linux Tests',
    console_view_entry = ci.console_view_entry(
        category = 'release',
        short_name = 'tst',
    ),
    goma_backend = None,
    triggered_by = [builder_name('Linux Builder')],
)

ci.linux_builder(
    name = 'linux-ozone-rel',
    console_view_entry = ci.console_view_entry(
        category = 'release',
        short_name = 'ozo',
    ),
)

ci.linux_builder(
    name = 'Linux Ozone Tester (Headless)',
    console_view = 'chromium.fyi',
    console_view_entry = ci.console_view_entry(
        category = 'linux',
        short_name = 'loh',
    ),
    goma_backend = None,
    triggered_by = [builder_name('linux-ozone-rel')],
)

ci.linux_builder(
    name = 'Linux Ozone Tester (Wayland)',
    console_view = 'chromium.fyi',
    console_view_entry = ci.console_view_entry(
        category = 'linux',
        short_name = 'low',
    ),
    goma_backend = None,
    triggered_by = [builder_name('linux-ozone-rel')],
)

ci.linux_builder(
    name = 'Linux Ozone Tester (X11)',
    console_view = 'chromium.fyi',
    console_view_entry = ci.console_view_entry(
        category = 'linux',
        short_name = 'lox',
    ),
    goma_backend = None,
    triggered_by = [builder_name('linux-ozone-rel')],
)


ci.mac_builder(
    name = 'Mac Builder',
    console_view_entry = ci.console_view_entry(
        category = 'release',
        short_name = 'bld',
    ),
    os = os.MAC_10_14,
)

# The build runs on 10.13, but triggers tests on 10.10 bots.
ci.mac_builder(
    name = 'Mac10.10 Tests',
    console_view_entry = ci.console_view_entry(
        category = 'release',
        short_name = '10',
    ),
    triggered_by = [builder_name('Mac Builder')],
)

# The build runs on 10.13, but triggers tests on 10.11 bots.
ci.mac_builder(
    name = 'Mac10.11 Tests',
    console_view_entry = ci.console_view_entry(
        category = 'release',
        short_name = '11',
    ),
    triggered_by = [builder_name('Mac Builder')],
)

ci.mac_builder(
    name = 'Mac10.12 Tests',
    console_view_entry = ci.console_view_entry(
        category = 'release',
        short_name = '12',
    ),
    os = os.MAC_10_12,
    triggered_by = [builder_name('Mac Builder')],
)

ci.mac_builder(
    name = 'Mac10.13 Tests',
    console_view_entry = ci.console_view_entry(
        category = 'release',
        short_name = '13',
    ),
    os = os.MAC_10_13,
    triggered_by = [builder_name('Mac Builder')],
)

ci.mac_builder(
    name = 'Mac10.14 Tests',
    console_view_entry = ci.console_view_entry(
        category = 'release',
        short_name = '14',
    ),
    os = os.MAC_10_14,
    triggered_by = [builder_name('Mac Builder')],
)

ci.mac_builder(
    name = 'WebKit Mac10.13 (retina)',
    console_view_entry = ci.console_view_entry(
        category = 'release',
        short_name = 'ret',
    ),
    os = os.MAC_10_13,
    triggered_by = [builder_name('Mac Builder')],
)


ci.mac_ios_builder(
    name = 'ios-simulator',
    console_view_entry = ci.console_view_entry(
        category = 'ios|default',
        short_name = 'sim',
    ),
    goma_backend = None,
)



ci.win_builder(
    name = 'Win 7 Tests x64 (1)',
    console_view_entry = ci.console_view_entry(
        category = 'release|tester',
        short_name = '64',
    ),
    os = os.WINDOWS_7,
    triggered_by = [builder_name('Win x64 Builder')],
)

ci.win_builder(
    name = 'Win x64 Builder',
    console_view_entry = ci.console_view_entry(
        category = 'release|builder',
        short_name = '64',
    ),
    cores = 32,
    os = os.WINDOWS_ANY,
)

ci.win_builder(
    name = 'Win10 Tests x64',
    console_view_entry = ci.console_view_entry(
        category = 'release|tester',
        short_name = 'w10',
    ),
    triggered_by = [builder_name('Win x64 Builder')],
)
