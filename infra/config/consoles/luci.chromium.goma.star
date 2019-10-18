luci.console_view(
    name = 'luci.chromium.goma',
    header = '//consoles/chromium-header.textpb',
    include_experimental_builds = True,
    repo = 'https://chromium.googlesource.com/chromium/src',
    entries = [
        luci.console_view_entry(
            builder = 'ci/Chromium Linux Goma Staging',
            category = 'clients5',
            short_name = 'lnx',
        ),
        luci.console_view_entry(
            builder = 'ci/Chromium Mac Goma Staging',
            category = 'clients5',
            short_name = 'mac',
        ),
        luci.console_view_entry(
            builder = 'ci/CrWinGomaStaging',
            category = 'clients5',
            short_name = 'win',
        ),
        luci.console_view_entry(
            builder = 'ci/Chromium Linux Goma RBE Staging (clobber)',
            category = 'rbe|rel',
            short_name = 'clb',
        ),
        luci.console_view_entry(
            builder = 'ci/Chromium Linux Goma RBE Staging',
            category = 'rbe|rel',
        ),
        luci.console_view_entry(
            builder = 'ci/Chromium Linux Goma RBE Staging (dbg) (clobber)',
            category = 'rbe|debug',
            short_name = 'clb',
        ),
        luci.console_view_entry(
            builder = 'ci/Chromium Linux Goma RBE Staging (dbg)',
            category = 'rbe|debug',
        ),
    ],
)
