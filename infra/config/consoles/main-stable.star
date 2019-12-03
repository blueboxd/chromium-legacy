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
        # TODO(https://crbug.com/922150) Move these to an appropriate console
        # and/or don't have linux-rel mirror these since they do not appear on
        # the main console
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
    ],
)
