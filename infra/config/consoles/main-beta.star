# TOD(https://crbug.com/922150) Add to chromium header
luci.console_view(
    name = 'main-beta',
    header = '//consoles/chromium-header.textpb',
    repo = 'https://chromium.googlesource.com/chromium/src',
    # TODO(gbeaty) Define the main consoles inside the respective versioned
    # directories once their contents are stablilized
    refs = ['refs/branch-heads/3945'],
    title = 'Chromium Beta Console',
    entries = [
        luci.console_view_entry(
            builder = 'ci-beta/Linux Builder',
            category = 'chromium.linux|release',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Linux Tests',
            category = 'chromium.linux|release',
            short_name = 'tst',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/linux-chromeos-rel',
            category = 'chromium.chromiumos|default',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/chromeos-amd64-generic-rel',
            category = 'chromium.chromiumos|simple|release|x64',
            short_name = 'rel',
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
            builder = 'ci-beta/WebKit Mac10.13 (retina)',
            category = 'chromium.mac|release',
            short_name = 'ret',
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
