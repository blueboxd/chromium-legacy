load('//lib/builders.star', 'builder_name', 'defaults')
load('../vars.star', 'vars')

defaults.bucket.set(vars.ci_bucket)

# TODO(https://crbug.com/922150) Add to chromium header
luci.console_view(
    name = vars.main_console_name,
    header = '//consoles/chromium-header.textpb',
    repo = 'https://chromium.googlesource.com/chromium/src',
    refs = [vars.ref],
    title = vars.main_console_title,
    entries = [
        luci.console_view_entry(
            builder = builder_name('Linux Builder'),
            category = 'chromium.linux|release',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = builder_name('Linux Ozone Tester (Headless)'),
            category = 'linux',
            short_name = 'loh',
        ),
        luci.console_view_entry(
            builder = builder_name('Linux Ozone Tester (Wayland)'),
            category = 'linux',
            short_name = 'low',
        ),
        luci.console_view_entry(
            builder = builder_name('Linux Ozone Tester (X11)'),
            category = 'linux',
            short_name = 'lox',
        ),
        luci.console_view_entry(
            builder = builder_name('Linux Tests'),
            category = 'chromium.linux|release',
            short_name = 'tst',
        ),
        luci.console_view_entry(
            builder = builder_name('linux-ozone-rel'),
            category = 'chromium.linux|release',
            short_name = 'ozo',
        ),
        luci.console_view_entry(
            builder = builder_name('Cast Linux'),
            category = 'chromium.linux|cast',
            short_name = 'vid',
        ),
        luci.console_view_entry(
            builder = builder_name('Fuchsia ARM64'),
            category = 'chromium.linux|fuchsia|a64',
        ),
        luci.console_view_entry(
            builder = builder_name('Fuchsia x64'),
            category = 'chromium.linux|fuchsia|x64',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = builder_name('linux-chromeos-rel'),
            category = 'chromium.chromiumos|default',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = builder_name('linux-chromeos-dbg'),
            category = 'chromium.chromiumos|default',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = builder_name('chromeos-arm-generic-rel'),
            category = 'chromium.chromiumos|simple|release',
            short_name = 'arm',
        ),
        luci.console_view_entry(
            builder = builder_name('chromeos-amd64-generic-rel'),
            category = 'chromium.chromiumos|simple|release|x64',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = builder_name('Mac Builder'),
            category = 'chromium.mac|release',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = builder_name('Mac10.10 Tests'),
            category = 'chromium.mac|release',
            short_name = '10',
        ),
        luci.console_view_entry(
            builder = builder_name('Mac10.11 Tests'),
            category = 'chromium.mac|release',
            short_name = '11',
        ),
        luci.console_view_entry(
            builder = builder_name('Mac10.12 Tests'),
            category = 'chromium.mac|release',
            short_name = '12',
        ),
        luci.console_view_entry(
            builder = builder_name('Mac10.13 Tests'),
            category = 'chromium.mac|release',
            short_name = '13',
        ),
        luci.console_view_entry(
            builder = builder_name('Mac10.14 Tests'),
            category = 'chromium.mac|release',
            short_name = '14',
        ),
        luci.console_view_entry(
            builder = builder_name('WebKit Mac10.13 (retina)'),
            category = 'chromium.mac|release',
            short_name = 'ret',
        ),
        luci.console_view_entry(
            builder = builder_name('Mac Builder (dbg)'),
            category = 'chromium.mac|debug',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = builder_name('Mac10.13 Tests (dbg)'),
            category = 'chromium.mac|debug',
            short_name = '13',
        ),
        luci.console_view_entry(
            builder = builder_name('ios-simulator'),
            category = 'chromium.mac|ios|default',
            short_name = 'sim',
        ),
        luci.console_view_entry(
            builder = builder_name('Win x64 Builder'),
            category = 'chromium.win|release|builder',
            short_name = '64',
        ),
        luci.console_view_entry(
            builder = builder_name('Win 7 Tests x64 (1)'),
            category = 'chromium.win|release|tester',
            short_name = '64',
        ),
        luci.console_view_entry(
            builder = builder_name('Win10 Tests x64'),
            category = 'chromium.win|release|tester',
            short_name = 'w10',
        ),
        luci.console_view_entry(
            builder = builder_name('Win Builder (dbg)'),
            category = 'chromium.win|debug|builder',
            short_name = '32',
        ),
        luci.console_view_entry(
            builder = builder_name('Win7 Tests (dbg)(1)'),
            category = 'chromium.win|debug|tester',
            short_name = '7',
        ),
        luci.console_view_entry(
            builder = builder_name('Linux ASan LSan Builder'),
            category = 'chromium.memory|linux|asan lsan',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = builder_name('Linux ASan LSan Tests (1)'),
            category = 'chromium.memory|linux|asan lsan',
            short_name = 'tst',
        ),
        luci.console_view_entry(
            builder = builder_name('Linux ASan Tests (sandboxed)'),
            category = 'chromium.memory|linux|asan lsan',
            short_name = 'sbx',
        ),
        # TODO(https://crbug.com/922150) Add the following builders to the main
        # console or don't have them be mirrored by main waterfall trybots
        luci.console_view_entry(
            builder = builder_name('android-kitkat-arm-rel'),
            category = 'chromium.android',
            short_name = 'k',
        ),
        luci.console_view_entry(
            builder = builder_name('android-marshmallow-arm64-rel'),
            category = 'chromium.android',
            short_name = 'm',
        ),
        luci.console_view_entry(
            builder = builder_name('Cast Android (dbg)'),
            category = 'chromium.android',
            short_name = 'cst',
        ),
        luci.console_view_entry(
            builder = builder_name('Android arm Builder (dbg)'),
            category = 'chromium.android|builder|arm',
            short_name = '32',
        ),
        luci.console_view_entry(
            builder = builder_name('android-cronet-arm-rel'),
            category = 'chromium.android|cronet|arm',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = builder_name('android-cronet-kitkat-arm-rel'),
            category = 'chromium.android|cronet|test',
            short_name = 'k',
        ),
        luci.console_view_entry(
            builder = builder_name('android-cronet-lollipop-arm-rel'),
            category = 'chromium.android|cronet|test',
            short_name = 'l',
        ),
        luci.console_view_entry(
            builder = builder_name('mac-osxbeta-rel'),
            category = 'chromium.fyi|mac',
            short_name = 'osxbeta',
        ),
        luci.console_view_entry(
            builder = builder_name('Win10 Tests x64 1803'),
            category = 'chromium.fyi|win10|1803',
        ),
        luci.console_view_entry(
            builder = builder_name('Android Release (Nexus 5X)'),
            category = 'chromium.gpu|android',
        ),
        luci.console_view_entry(
            builder = builder_name('GPU Linux Builder'),
            category = 'chromium.gpu|linux',
        ),
        luci.console_view_entry(
            builder = builder_name('Linux Release (NVIDIA)'),
            category = 'chromium.gpu|linux',
        ),
        luci.console_view_entry(
            builder = builder_name('GPU Mac Builder'),
            category = 'chromium.gpu|mac',
        ),
        luci.console_view_entry(
            builder = builder_name('Mac Release (Intel)'),
            category = 'chromium.gpu|mac',
        ),
        luci.console_view_entry(
            builder = builder_name('Mac Retina Release (AMD)'),
            category = 'chromium.gpu|mac',
        ),
        luci.console_view_entry(
            builder = builder_name('GPU Win x64 Builder'),
            category = 'chromium.gpu|win',
        ),
        luci.console_view_entry(
            builder = builder_name('Win10 x64 Release (NVIDIA)'),
            category = 'chromium.gpu|win',
        ),
    ],
)
