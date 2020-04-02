load('//lib/builders.star', 'builder_name', 'defaults')
load('../vars.star', 'vars')

defaults.bucket.set(vars.ci_bucket)

luci.console_view(
    name = vars.main_console_name,
    header = '//consoles/chromium-header.textpb',
    repo = 'https://chromium.googlesource.com/chromium/src',
    refs = [vars.ref],
    title = vars.main_console_title,
    entries = [
        luci.console_view_entry(
            builder = builder_name('android-archive-dbg'),
            category = 'chromium|android',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = builder_name('android-archive-rel'),
            category = 'chromium|android',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = builder_name('linux-archive-dbg'),
            category = 'chromium|linux',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = builder_name('linux-archive-rel'),
            category = 'chromium|linux',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = builder_name('mac-archive-dbg'),
            category = 'chromium|mac',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = builder_name('mac-archive-rel'),
            category = 'chromium|mac',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = builder_name('win32-archive-rel'),
            category = 'chromium|win|rel',
            short_name = '32',
        ),
        luci.console_view_entry(
            builder = builder_name('win-archive-rel'),
            category = 'chromium|win|rel',
            short_name = '64',
        ),
        luci.console_view_entry(
            builder = builder_name('win32-archive-dbg'),
            category = 'chromium|win|dbg',
            short_name = '32',
        ),
        luci.console_view_entry(
            builder = builder_name('win-archive-dbg'),
            category = 'chromium|win|dbg',
            short_name = '64',
        ),
        luci.console_view_entry(
            builder = builder_name('Win Builder'),
            category = 'chromium.win|release|builder',
            short_name = '32',
        ),
        luci.console_view_entry(
            builder = builder_name('Win x64 Builder'),
            category = 'chromium.win|release|builder',
            short_name = '64',
        ),
        luci.console_view_entry(
            builder = builder_name('Win7 (32) Tests'),
            category = 'chromium.win|release|tester',
            short_name = '32',
        ),
        luci.console_view_entry(
            builder = builder_name('Win7 Tests (1)'),
            category = 'chromium.win|release|tester',
            short_name = '32',
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
            builder = builder_name('Win x64 Builder (dbg)'),
            category = 'chromium.win|debug|builder',
            short_name = '64',
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
            builder = builder_name('Win10 Tests x64 (dbg)'),
            category = 'chromium.win|debug|tester',
            short_name = '10',
        ),
        luci.console_view_entry(
            builder = builder_name('Windows deterministic'),
            category = 'chromium.win|misc',
            short_name = 'det',
        ),
        luci.console_view_entry(
            builder = builder_name('WebKit Win10'),
            category = 'chromium.win|misc',
            short_name = 'wbk',
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
            builder = builder_name('ios-device'),
            category = 'chromium.mac|ios|default',
            short_name = 'dev',
        ),
        luci.console_view_entry(
            builder = builder_name('ios-simulator'),
            category = 'chromium.mac|ios|default',
            short_name = 'sim',
        ),
        luci.console_view_entry(
            builder = builder_name('ios-simulator-full-configs'),
            category = 'chromium.mac|ios|default',
            short_name = 'ful',
        ),
        luci.console_view_entry(
            builder = builder_name('ios-simulator-noncq'),
            category = 'chromium.mac|ios|default',
            short_name = 'non',
        ),
        luci.console_view_entry(
            builder = builder_name('Linux Builder'),
            category = 'chromium.linux|release',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = builder_name('Linux Tests'),
            category = 'chromium.linux|release',
            short_name = 'tst',
        ),
        luci.console_view_entry(
            builder = builder_name('Network Service Linux'),
            category = 'chromium.linux|release',
            short_name = 'nsl',
        ),
        luci.console_view_entry(
            builder = builder_name('linux-gcc-rel'),
            category = 'chromium.linux|release',
            short_name = 'gcc',
        ),
        luci.console_view_entry(
            builder = builder_name('Deterministic Linux'),
            category = 'chromium.linux|release',
            short_name = 'det',
        ),
        luci.console_view_entry(
            builder = builder_name('linux-ozone-rel'),
            category = 'chromium.linux|release',
            short_name = 'ozo',
        ),
        luci.console_view_entry(
            builder = builder_name('linux-trusty-rel'),
            category = 'chromium.linux|release',
            short_name = 'tru',
        ),
        luci.console_view_entry(
            builder = builder_name('Linux Builder (dbg)(32)'),
            category = 'chromium.linux|debug|builder',
            short_name = '32',
        ),
        luci.console_view_entry(
            builder = builder_name('Linux Builder (dbg)'),
            category = 'chromium.linux|debug|builder',
            short_name = '64',
        ),
        luci.console_view_entry(
            builder = builder_name('Deterministic Linux (dbg)'),
            category = 'chromium.linux|debug|builder',
            short_name = 'det',
        ),
        luci.console_view_entry(
            builder = builder_name('Linux Tests (dbg)(1)'),
            category = 'chromium.linux|debug|tester',
            short_name = '64',
        ),
        luci.console_view_entry(
            builder = builder_name('Cast Linux'),
            category = 'chromium.linux|cast',
            short_name = 'vid',
        ),
        luci.console_view_entry(
            builder = builder_name('Cast Audio Linux'),
            category = 'chromium.linux|cast',
            short_name = 'aud',
        ),
        luci.console_view_entry(
            builder = builder_name('Fuchsia ARM64'),
            category = 'chromium.linux|fuchsia|a64',
        ),
        luci.console_view_entry(
            builder = builder_name('fuchsia-arm64-cast'),
            category = 'chromium.linux|fuchsia|cast',
            short_name = 'a64',
        ),
        luci.console_view_entry(
            builder = builder_name('fuchsia-x64-cast'),
            category = 'chromium.linux|fuchsia|cast',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = builder_name('Deterministic Fuchsia (dbg)'),
            category = 'chromium.linux|fuchsia|x64',
            short_name = 'det',
        ),
        luci.console_view_entry(
            builder = builder_name('Fuchsia x64'),
            category = 'chromium.linux|fuchsia|x64',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = builder_name('Linux ChromiumOS Full'),
            category = 'chromium.chromiumos|default',
            short_name = 'ful',
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
            builder = builder_name('chromeos-amd64-generic-asan-rel'),
            category = 'chromium.chromiumos|simple|release|x64',
            short_name = 'asn',
        ),
        luci.console_view_entry(
            builder = builder_name('chromeos-amd64-generic-cfi-thin-lto-rel'),
            category = 'chromium.chromiumos|simple|release|x64',
            short_name = 'cfi',
        ),
        luci.console_view_entry(
            builder = builder_name('chromeos-amd64-generic-dbg'),
            category = 'chromium.chromiumos|simple|debug|x64',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = builder_name('chromeos-amd64-generic-rel'),
            category = 'chromium.chromiumos|simple|release|x64',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = builder_name('chromeos-arm-generic-dbg'),
            category = 'chromium.chromiumos|simple|debug',
            short_name = 'arm',
        ),
        luci.console_view_entry(
            builder = builder_name('chromeos-arm-generic-rel'),
            category = 'chromium.chromiumos|simple|release',
            short_name = 'arm',
        ),
        luci.console_view_entry(
            builder = builder_name('chromeos-kevin-rel'),
            category = 'chromium.chromiumos|simple|release',
            short_name = 'kvn',
        ),
        luci.console_view_entry(
            builder = builder_name('android-cronet-arm-dbg'),
            category = 'chromium.android|cronet|arm',
            short_name = 'dbg',
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
            builder = builder_name('Android arm Builder (dbg)'),
            category = 'chromium.android|builder|arm',
            short_name = '32',
        ),
        luci.console_view_entry(
            builder = builder_name('Android arm64 Builder (dbg)'),
            category = 'chromium.android|builder|arm',
            short_name = '64',
        ),
        luci.console_view_entry(
            builder = builder_name('Android x86 Builder (dbg)'),
            category = 'chromium.android|builder|x86',
            short_name = '32',
        ),
        luci.console_view_entry(
            builder = builder_name('Android x64 Builder (dbg)'),
            category = 'chromium.android|builder|x86',
            short_name = '64',
        ),
        luci.console_view_entry(
            builder = builder_name('KitKat Phone Tester (dbg)'),
            category = 'chromium.android|tester|phone',
            short_name = 'K',
        ),
        luci.console_view_entry(
            builder = builder_name('Lollipop Phone Tester'),
            category = 'chromium.android|tester|phone',
            short_name = 'L',
        ),
        luci.console_view_entry(
            builder = builder_name('Marshmallow 64 bit Tester'),
            category = 'chromium.android|tester|phone',
            short_name = 'M',
        ),
        luci.console_view_entry(
            builder = builder_name('Nougat Phone Tester'),
            category = 'chromium.android|tester|phone',
            short_name = 'N',
        ),
        luci.console_view_entry(
            builder = builder_name('Oreo Phone Tester'),
            category = 'chromium.android|tester|phone',
            short_name = 'O',
        ),
        luci.console_view_entry(
            builder = builder_name('android-pie-arm64-dbg'),
            category = 'chromium.android|tester|phone',
            short_name = 'P',
        ),
        luci.console_view_entry(
            builder = builder_name('KitKat Tablet Tester'),
            category = 'chromium.android|tester|tablet',
            short_name = 'K',
        ),
        luci.console_view_entry(
            builder = builder_name('Lollipop Tablet Tester'),
            category = 'chromium.android|tester|tablet',
            short_name = 'L',
        ),
        luci.console_view_entry(
            builder = builder_name('Marshmallow Tablet Tester'),
            category = 'chromium.android|tester|tablet',
            short_name = 'M',
        ),
        luci.console_view_entry(
            builder = builder_name('Android WebView L (dbg)'),
            category = 'chromium.android|tester|webview',
            short_name = 'L',
        ),
        luci.console_view_entry(
            builder = builder_name('Android WebView M (dbg)'),
            category = 'chromium.android|tester|webview',
            short_name = 'M',
        ),
        luci.console_view_entry(
            builder = builder_name('Android WebView N (dbg)'),
            category = 'chromium.android|tester|webview',
            short_name = 'N',
        ),
        luci.console_view_entry(
            builder = builder_name('Android WebView O (dbg)'),
            category = 'chromium.android|tester|webview',
            short_name = 'O',
        ),
        luci.console_view_entry(
            builder = builder_name('Android WebView P (dbg)'),
            category = 'chromium.android|tester|webview',
            short_name = 'P',
        ),
        luci.console_view_entry(
            builder = builder_name('android-kitkat-arm-rel'),
            category = 'chromium.android|on_cq',
            short_name = 'K',
        ),
        luci.console_view_entry(
            builder = builder_name('android-marshmallow-arm64-rel'),
            category = 'chromium.android|on_cq',
            short_name = 'M',
        ),
        luci.console_view_entry(
            builder = builder_name('Cast Android (dbg)'),
            category = 'chromium.android|on_cq',
            short_name = 'cst',
        ),
        luci.console_view_entry(
            builder = builder_name('android-pie-arm64-rel'),
            category = 'chromium.android|on_cq',
            short_name = 'P',
        ),
        luci.console_view_entry(
            builder = 'chrome:ci/linux-chromeos-chrome',
            category = 'chrome',
            short_name = 'cro',
        ),
        luci.console_view_entry(
            builder = 'chrome:ci/linux-chrome',
            category = 'chrome',
            short_name = 'lnx',
        ),
        luci.console_view_entry(
            builder = 'chrome:ci/mac-chrome',
            category = 'chrome',
            short_name = 'mac',
        ),
        luci.console_view_entry(
            builder = 'chrome:ci/win-chrome',
            category = 'chrome',
            short_name = 'win',
        ),
        luci.console_view_entry(
            builder = builder_name('win-asan'),
            category = 'chromium.memory|win',
            short_name = 'asn',
        ),
        luci.console_view_entry(
            builder = builder_name('Mac ASan 64 Builder'),
            category = 'chromium.memory|mac',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = builder_name('Mac ASan 64 Tests (1)'),
            category = 'chromium.memory|mac',
            short_name = 'tst',
        ),
        luci.console_view_entry(
            builder = builder_name('Linux TSan Builder'),
            category = 'chromium.memory|linux|TSan v2',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = builder_name('Linux TSan Tests'),
            category = 'chromium.memory|linux|TSan v2',
            short_name = 'tst',
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
        luci.console_view_entry(
            builder = builder_name('Linux MSan Builder'),
            category = 'chromium.memory|linux|msan',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = builder_name('Linux MSan Tests'),
            category = 'chromium.memory|linux|msan',
            short_name = 'tst',
        ),
        luci.console_view_entry(
            builder = builder_name('WebKit Linux ASAN'),
            category = 'chromium.memory|linux|webkit',
            short_name = 'asn',
        ),
        luci.console_view_entry(
            builder = builder_name('WebKit Linux MSAN'),
            category = 'chromium.memory|linux|webkit',
            short_name = 'msn',
        ),
        luci.console_view_entry(
            builder = builder_name('WebKit Linux Leak'),
            category = 'chromium.memory|linux|webkit',
            short_name = 'lk',
        ),
        luci.console_view_entry(
            builder = builder_name('Linux Chromium OS ASan LSan Builder'),
            category = 'chromium.memory|cros|asan',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = builder_name('Linux Chromium OS ASan LSan Tests (1)'),
            category = 'chromium.memory|cros|asan',
            short_name = 'tst',
        ),
        luci.console_view_entry(
            builder = builder_name('Linux ChromiumOS MSan Builder'),
            category = 'chromium.memory|cros|msan',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = builder_name('Linux ChromiumOS MSan Tests'),
            category = 'chromium.memory|cros|msan',
            short_name = 'tst',
        ),
        luci.console_view_entry(
            builder = builder_name('android-asan'),
            category = 'chromium.memory|android',
            short_name = 'asn',
        ),
        luci.console_view_entry(
            builder = builder_name('Linux CFI'),
            category = 'chromium.memory|cfi',
            short_name = 'lnx',
        ),
        luci.console_view_entry(
            builder = builder_name('Dawn Linux x64 DEPS Builder'),
            category = 'chromium.dawn|DEPS|Linux|Builder',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = builder_name('Dawn Linux x64 DEPS Release (Intel HD 630)'),
            category = 'chromium.dawn|DEPS|Linux|Intel',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = builder_name('Dawn Linux x64 DEPS Release (NVIDIA)'),
            category = 'chromium.dawn|DEPS|Linux|Nvidia',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = builder_name('Dawn Mac x64 DEPS Builder'),
            category = 'chromium.dawn|DEPS|Mac|Builder',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = builder_name('Dawn Mac x64 DEPS Release (AMD)'),
            category = 'chromium.dawn|DEPS|Mac|AMD',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = builder_name('Dawn Mac x64 DEPS Release (Intel)'),
            category = 'chromium.dawn|DEPS|Mac|Intel',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = builder_name('Dawn Win10 x86 DEPS Builder'),
            category = 'chromium.dawn|DEPS|Windows|Builder',
            short_name = 'x86',
        ),
        luci.console_view_entry(
            builder = builder_name('Dawn Win10 x64 DEPS Builder'),
            category = 'chromium.dawn|DEPS|Windows|Builder',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = builder_name('Dawn Win10 x86 DEPS Release (Intel HD 630)'),
            category = 'chromium.dawn|DEPS|Windows|Intel',
            short_name = 'x86',
        ),
        luci.console_view_entry(
            builder = builder_name('Dawn Win10 x64 DEPS Release (Intel HD 630)'),
            category = 'chromium.dawn|DEPS|Windows|Intel',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = builder_name('Dawn Win10 x86 DEPS Release (NVIDIA)'),
            category = 'chromium.dawn|DEPS|Windows|Nvidia',
            short_name = 'x86',
        ),
        luci.console_view_entry(
            builder = builder_name('Dawn Win10 x64 DEPS Release (NVIDIA)'),
            category = 'chromium.dawn|DEPS|Windows|Nvidia',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = builder_name('GPU Mac Builder'),
            category = 'chromium.gpu|Mac',
        ),
        luci.console_view_entry(
            builder = builder_name('Mac Release (Intel)'),
            category = 'chromium.gpu|Mac',
        ),
        luci.console_view_entry(
            builder = builder_name('Mac Retina Release (AMD)'),
            category = 'chromium.gpu|Mac',
        ),
        luci.console_view_entry(
            builder = builder_name('GPU Linux Builder'),
            category = 'chromium.gpu|Linux',
        ),
        luci.console_view_entry(
            builder = builder_name('Linux Release (NVIDIA)'),
            category = 'chromium.gpu|Linux',
        ),
        luci.console_view_entry(
            builder = builder_name('Android Release (Nexus 5X)'),
            category = 'chromium.gpu|Android',
        ),
        # TODO(gbeaty): FYI builders should not be mirrors for try builders on
        # the CQ, but things are what they are
        luci.console_view_entry(
            builder = builder_name('ios-simulator-cronet'),
            category = 'chromium.fyi|cronet',
        ),
        luci.console_view_entry(
            builder = builder_name('mac-osxbeta-rel'),
            category = 'chromium.fyi|mac',
            short_name = 'beta',
        ),
        luci.console_view_entry(
            builder = builder_name('chromeos-kevin-rel-hw-tests'),
            category = 'chromium.fyi|chromos',
        ),
        luci.console_view_entry(
            builder = builder_name('VR Linux'),
            category = 'chromium.fyi|linux',
        ),
        luci.console_view_entry(
            builder = builder_name('Linux Ozone Tester (Wayland)'),
            category = 'chromium.fyi|linux',
            short_name = 'low',
        ),
        luci.console_view_entry(
            builder = builder_name('Linux Ozone Tester (X11)'),
            category = 'chromium.fyi|linux',
            short_name = 'lox',
        ),
    ],
)
