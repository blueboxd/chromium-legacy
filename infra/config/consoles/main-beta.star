# TOD(https://crbug.com/922150) Add to chromium header
luci.console_view(
    name = 'main-beta',
    header = '//consoles/chromium-header.textpb',
    repo = 'https://chromium.googlesource.com/chromium/src',
    # TODO(gbeaty) Define the main consoles inside the respective versioned
    # directories once their contents are stablilized
    refs = ['refs/branch-heads/4085'],
    title = 'Chromium Beta Console',
    entries = [
        luci.console_view_entry(
            builder = 'ci-beta/Linux Builder',
            category = 'chromium.linux|release',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Linux Ozone Tester (Wayland)',
            category = 'linux',
            short_name = 'low',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Linux Ozone Tester (X11)',
            category = 'linux',
            short_name = 'lox',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Linux Tests',
            category = 'chromium.linux|release',
            short_name = 'tst',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/linux-ozone-rel',
            category = 'chromium.linux|release',
            short_name = 'ozo',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Linux Builder (dbg)',
            category = 'chromium.linux|debug|builder',
            short_name = '64',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Linux Tests (dbg)(1)',
            category = 'chromium.linux|debug|tester',
            short_name = '64',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Cast Linux',
            category = 'chromium.linux|cast',
            short_name = 'vid',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/fuchsia-arm64-cast',
            category = 'chromium.linux|fuchsia|cast',
            short_name = 'a64',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/fuchsia-x64-cast',
            category = 'chromium.linux|fuchsia|cast',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/fuchsia-x64-dbg',
            category = 'chromium.linux|fuchsia|x64',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Fuchsia ARM64',
            category = 'chromium.linux|fuchsia|a64',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Fuchsia x64',
            category = 'chromium.linux|fuchsia|x64',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/linux-chromeos-rel',
            category = 'chromium.chromiumos|default',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/linux-chromeos-dbg',
            category = 'chromium.chromiumos|default',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/chromeos-arm-generic-rel',
            category = 'chromium.chromiumos|simple|release',
            short_name = 'arm',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/chromeos-amd64-generic-dbg',
            category = 'chromium.chromiumos|simple|debug|x64',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/chromeos-amd64-generic-rel',
            category = 'chromium.chromiumos|simple|release|x64',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/chromeos-kevin-rel',
            category = 'chromium.chromiumos|simple|release',
            short_name = 'kvn',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Mac Builder',
            category = 'chromium.mac|release',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Mac10.10 Tests',
            category = 'chromium.mac|release',
            short_name = '10',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Mac10.11 Tests',
            category = 'chromium.mac|release',
            short_name = '11',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Mac10.12 Tests',
            category = 'chromium.mac|release',
            short_name = '12',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Mac10.13 Tests',
            category = 'chromium.mac|release',
            short_name = '13',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Mac10.14 Tests',
            category = 'chromium.mac|release',
            short_name = '14',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/WebKit Mac10.13 (retina)',
            category = 'chromium.mac|release',
            short_name = 'ret',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Mac Builder (dbg)',
            category = 'chromium.mac|debug',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Mac10.13 Tests (dbg)',
            category = 'chromium.mac|debug',
            short_name = '13',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/ios-simulator',
            category = 'chromium.mac|ios|default',
            short_name = 'sim',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Win x64 Builder',
            category = 'chromium.win|release|builder',
            short_name = '64',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Win 7 Tests x64 (1)',
            category = 'chromium.win|release|tester',
            short_name = '64',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Win10 Tests x64',
            category = 'chromium.win|release|tester',
            short_name = 'w10',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Win Builder (dbg)',
            category = 'chromium.win|debug|builder',
            short_name = '32',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Win7 Tests (dbg)(1)',
            category = 'chromium.win|debug|tester',
            short_name = '7',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Linux ASan LSan Builder',
            category = 'chromium.memory|linux|asan lsan',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Linux ASan LSan Tests (1)',
            category = 'chromium.memory|linux|asan lsan',
            short_name = 'tst',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Linux ASan Tests (sandboxed)',
            category = 'chromium.memory|linux|asan lsan',
            short_name = 'sbx',
        ),

        # TODO(https://crbug.com/922150) Add the following builders to the main
        # console or don't have them be mirrored by main waterfall trybots
        luci.console_view_entry(
            builder = 'ci-beta/android-kitkat-arm-rel',
            category = 'chromium.android',
            short_name = 'k',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/android-marshmallow-arm64-rel',
            category = 'chromium.android',
            short_name = 'm',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Cast Android (dbg)',
            category = 'chromium.android',
            short_name = 'cst',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Android arm Builder (dbg)',
            category = 'chromium.android|builder|arm',
            short_name = '32',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Android arm64 Builder (dbg)',
            category = 'chromium.android|builder|arm',
            short_name = '64',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Android x86 Builder (dbg)',
            category = 'chromium.android|builder|x86',
            short_name = '32',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Android x64 Builder (dbg)',
            category = 'chromium.android|builder|x86',
            short_name = '64',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Marshmallow 64 bit Tester',
            category = 'chromium.android|tester|phone',
            short_name = 'M',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Nougat Phone Tester',
            category = 'chromium.android|tester|phone',
            short_name = 'N',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Oreo Phone Tester',
            category = 'chromium.android|tester|phone',
            short_name = 'O',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/android-pie-arm64-dbg',
            category = 'chromium.android|tester|phone',
            short_name = 'P',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/android-pie-arm64-rel',
            category = 'chromium.android|on_cq',
            short_name = 'P',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Android WebView M (dbg)',
            category = 'chromium.android|tester|webview',
            short_name = 'M',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Android WebView N (dbg)',
            category = 'chromium.android|tester|webview',
            short_name = 'N',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Android WebView O (dbg)',
            category = 'chromium.android|tester|webview',
            short_name = 'O',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Android WebView P (dbg)',
            category = 'chromium.android|tester|webview',
            short_name = 'P',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/android-cronet-arm-rel',
            category = 'chromium.android|cronet|arm',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/android-cronet-arm-dbg',
            category = 'cronet|arm',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/android-cronet-kitkat-arm-rel',
            category = 'chromium.android|cronet|test',
            short_name = 'k',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/android-cronet-lollipop-arm-rel',
            category = 'chromium.android|cronet|test',
            short_name = 'l',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Dawn Linux x64 DEPS Builder',
            category = 'chromium.dawn|DEPS|Linux|Builder',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Dawn Linux x64 DEPS Release (Intel HD 630)',
            category = 'chromium.dawn|DEPS|Linux|Intel',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Dawn Linux x64 DEPS Release (NVIDIA)',
            category = 'chromium.dawn|DEPS|Linux|Nvidia',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Dawn Mac x64 DEPS Builder',
            category = 'chromium.dawn|DEPS|Mac|Builder',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Dawn Mac x64 DEPS Release (AMD)',
            category = 'chromium.dawn|DEPS|Mac|AMD',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Dawn Mac x64 DEPS Release (Intel)',
            category = 'chromium.dawn|DEPS|Mac|Intel',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Dawn Win10 x86 DEPS Builder',
            category = 'chromium.dawn|DEPS|Windows|Builder',
            short_name = 'x86',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Dawn Win10 x64 DEPS Builder',
            category = 'chromium.dawn|DEPS|Windows|Builder',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Dawn Win10 x86 DEPS Release (Intel HD 630)',
            category = 'chromium.dawn|DEPS|Windows|Intel',
            short_name = 'x86',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Dawn Win10 x64 DEPS Release (Intel HD 630)',
            category = 'chromium.dawn|DEPS|Windows|Intel',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Dawn Win10 x86 DEPS Release (NVIDIA)',
            category = 'chromium.dawn|DEPS|Windows|Nvidia',
            short_name = 'x86',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Dawn Win10 x64 DEPS Release (NVIDIA)',
            category = 'chromium.dawn|DEPS|Windows|Nvidia',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/chromeos-kevin-rel-hw-tests',
            category = 'chromium.fyi|chromeos',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/VR Linux',
            category = 'chromium.fyi|linux',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/mac-osxbeta-rel',
            category = 'chromium.fyi|mac',
            short_name = 'osxbeta',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Win10 Tests x64 1803',
            category = 'chromium.fyi|win10|1803',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Android Release (Nexus 5X)',
            category = 'chromium.gpu|android',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/GPU Linux Builder',
            category = 'chromium.gpu|linux',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Linux Release (NVIDIA)',
            category = 'chromium.gpu|linux',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/GPU Mac Builder',
            category = 'chromium.gpu|mac',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Mac Release (Intel)',
            category = 'chromium.gpu|mac',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Mac Retina Release (AMD)',
            category = 'chromium.gpu|mac',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/GPU Win x64 Builder',
            category = 'chromium.gpu|win',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Win10 x64 Release (NVIDIA)',
            category = 'chromium.gpu|win',
        ),
    ],
)
