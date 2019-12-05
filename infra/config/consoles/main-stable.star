# TOD(https://crbug.com/922150) Add to chromium header
luci.console_view(
    name = 'main-stable',
    header = '//consoles/chromium-header.textpb',
    repo = 'https://chromium.googlesource.com/chromium/src',
    # TODO(gbeaty) Define the main consoles inside the respective versioned
    # directories once their contents are stablilized
    refs = ['refs/branch-heads/3904'],
    title = 'Chromium Stable Console',
    entries = [
        luci.console_view_entry(
            builder = 'ci-stable/Linux Builder',
            category = 'chromium.linux|release',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = 'ci-stable/Linux Tests',
            category = 'chromium.linux|release',
            short_name = 'tst',
        ),
        luci.console_view_entry(
            builder = 'ci-stable/linux-chromeos-rel',
            category = 'chromium.chromiumos|default',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci-stable/chromeos-amd64-generic-rel',
            category = 'chromium.chromiumos|simple|release|x64',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci-stable/Mac Builder',
            category = 'chromium.mac|release',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = 'ci-stable/Mac10.10 Tests',
            category = 'chromium.mac|release',
            short_name = '10',
        ),
        luci.console_view_entry(
            builder = 'ci-stable/Mac10.11 Tests',
            category = 'chromium.mac|release',
            short_name = '11',
        ),
        luci.console_view_entry(
            builder = 'ci-stable/Mac10.12 Tests',
            category = 'chromium.mac|release',
            short_name = '12',
        ),
        luci.console_view_entry(
            builder = 'ci-stable/Mac10.13 Tests',
            category = 'chromium.mac|release',
            short_name = '13',
        ),
        luci.console_view_entry(
            builder = 'ci-stable/WebKit Mac10.13 (retina)',
            category = 'chromium.mac|release',
            short_name = 'ret',
        ),
        luci.console_view_entry(
            builder = 'ci-stable/ios-simulator',
            category = 'chromium.mac|ios|default',
            short_name = 'sim',
        ),
        luci.console_view_entry(
            builder = 'ci-stable/Win x64 Builder',
            category = 'chromium.win|release|builder',
            short_name = '64',
        ),
        luci.console_view_entry(
            builder = 'ci-stable/Win 7 Tests x64 (1)',
            category = 'chromium.win|release|tester',
            short_name = '64',
        ),
        luci.console_view_entry(
            builder = 'ci-stable/Win10 Tests x64',
            category = 'chromium.win|release|tester',
            short_name = 'w10',
        ),
        # TODO(https://crbug.com/922150) Add the following builders to the main
        # console or don't have them be mirrored by main waterfall trybots
        luci.console_view_entry(
            builder = 'ci-stable/android-kitkat-arm-rel',
            category = 'chromium.android',
            short_name = 'k',
        ),
        luci.console_view_entry(
            builder = 'ci-stable/android-marshmallow-arm64-rel',
            category = 'chromium.android',
            short_name = 'm',
        ),
        luci.console_view_entry(
            builder = 'ci-stable/mac-osxbeta-rel',
            category = 'chromium.fyi|mac',
            short_name = 'osxbeta',
        ),
        luci.console_view_entry(
            builder = 'ci-stable/Win10 Tests x64 1803',
            category = 'chromium.fyi|win10|1803',
        ),
        luci.console_view_entry(
            builder = 'ci-stable/Android Release (Nexus 5X)',
            category = 'chromium.gpu|android',
        ),
        luci.console_view_entry(
            builder = 'ci-stable/GPU Linux Builder',
            category = 'chromium.gpu|linux',
        ),
        luci.console_view_entry(
            builder = 'ci-stable/Linux Release (NVIDIA)',
            category = 'chromium.gpu|linux',
        ),
        luci.console_view_entry(
            builder = 'ci-stable/GPU Mac Builder',
            category = 'chromium.gpu|mac',
        ),
        luci.console_view_entry(
            builder = 'ci-stable/Mac Release (Intel)',
            category = 'chromium.gpu|mac',
        ),
        luci.console_view_entry(
            builder = 'ci-stable/Mac Retina Release (AMD)',
            category = 'chromium.gpu|mac',
        ),
        luci.console_view_entry(
            builder = 'ci-stable/GPU Win x64 Builder',
            category = 'chromium.gpu|win',
        ),
        luci.console_view_entry(
            builder = 'ci-stable/Win10 x64 Release (NVIDIA)',
            category = 'chromium.gpu|win',
        ),
    ],
)
