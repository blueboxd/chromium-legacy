luci.console_view(
    name = 'chromium.android.fyi',
    header = '//consoles/chromium-header.textpb',
    repo = 'https://chromium.googlesource.com/chromium/src',
    entries = [
        luci.console_view_entry(
            builder = 'ci/android-bfcache-rel',
            category = 'android',
        ),
        # Formerly on chromium.memory. Moved to the FYI console for persistent
        # redness. https://crbug.com/1008094
        luci.console_view_entry(
            builder = 'ci/Android CFI',
            category = 'memory',
            short_name = 'cfi',
        ),
        luci.console_view_entry(
            builder = 'ci/Android WebLayer P FYI (rel)',
            category = 'weblayer',
            short_name = 'p-rel',
        ),
        luci.console_view_entry(
            builder = 'ci/Android WebView P Blink-CORS FYI (rel)',
            category = 'webview',
            short_name = 'cors',
        ),
        luci.console_view_entry(
            builder = 'ci/Android WebView P FYI (rel)',
            category = 'webview',
            short_name = 'p-rel',
        ),
        luci.console_view_entry(
            builder = 'ci/android-marshmallow-x86-fyi-rel',
            category = 'emulator|M|x86',
            short_name = 'rel',
        ),
        # TODO(hypan): remove this once there is no associaled disabled tests
        luci.console_view_entry(
            builder = 'ci/android-pie-x86-fyi-rel',
            category = 'emulator|P|x86',
            short_name = 'rel',
        ),
    ],
)
