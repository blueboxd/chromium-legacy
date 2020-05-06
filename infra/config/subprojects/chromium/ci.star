load('//lib/builders.star', 'cpu', 'goma', 'os', 'xcode_cache')
load('//lib/ci.star', 'ci')
load('./versioned/trunk/vars.star', 'vars')

# Execute the versioned files to define all of the per-branch entities
# (bucket, builders, console, poller, etc.)
exec('./versioned/trunk/buckets/ci.star')
exec('./versioned/m81/buckets/ci.star')
exec('./versioned/m83/buckets/ci.star')


ci.set_defaults(
    vars,
    add_to_console_view = True,
)


# *** After this point everything is trunk only ***
ci.console_view(
    name = 'chromium',
    include_experimental_builds = True,
    ordering = {},
)

ci.console_view(
    name = 'chromium.android',
    ordering = {
        None: ['cronet', 'builder', 'tester'],
        '*cpu*': ['arm', 'arm64', 'x86'],
        'cronet': '*cpu*',
        'builder': '*cpu*',
        'builder|det': ci.ordering(short_names=['rel', 'dbg']),
        'tester': ['phone', 'tablet'],
        'builder_tester|arm64': ci.ordering(short_names=['M proguard']),
    },
)

ci.console_view(
    name = 'chromium.android.fyi',
    ordering = {
        None: ['android', 'memory', 'weblayer', 'webview'],
    },
)

ci.console_view(
    name = 'chromium.chromiumos',
    ordering = {
        None: ['default'],
        'default': ci.ordering(short_names=['ful', 'rel']),
        'simple': ['release', 'debug'],
    },
)

ci.console_view(
    name = 'chromium.clang',
    ordering = {
        None: [
            'ToT Linux',
            'ToT Android',
            'ToT Mac',
            'ToT Windows',
            'ToT Code Coverage',
        ],
        'ToT Linux': ci.ordering(
            short_names=['rel', 'ofi', 'dbg', 'asn', 'fuz', 'msn', 'tsn'],
        ),
        'ToT Android': ci.ordering(short_names=['rel', 'dbg', 'x64']),
        'ToT Mac': ci.ordering(short_names=['rel', 'ofi', 'dbg']),
        'ToT Windows': ci.ordering(
            short_names=['rel', 'ofi'],
            categories=['x64'],
        ),
        'ToT Windows|x64': ci.ordering(short_names=['rel']),
        'CFI|Win': ci.ordering(short_names=['x86', 'x64']),
        'iOS': ['public'],
        'iOS|public': ci.ordering(short_names=['sim', 'dev']),
    },
    entries = [
        luci.console_view_entry(
            builder = 'chrome:ci/ToTLinuxOfficial',
            category = 'ToT Linux',
            short_name = 'ofi',
        ),
        luci.console_view_entry(
            builder = 'chrome:ci/ToTMacOfficial',
            category = 'ToT Mac',
            short_name = 'ofi',
        ),
        luci.console_view_entry(
            builder = 'chrome:ci/ToTWin',
            category = 'ToT Windows',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'chrome:ci/ToTWin64',
            category = 'ToT Windows|x64',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'chrome:ci/ToTWinOfficial',
            category = 'ToT Windows',
            short_name = 'ofi',
        ),
        luci.console_view_entry(
            builder = 'chrome:ci/ToTWinThinLTO64',
            category = 'ToT Windows|x64',
            short_name = 'lto',
        ),
        luci.console_view_entry(
            builder = 'chrome:ci/clang-tot-device',
            category = 'iOS|internal',
            short_name = 'dev',
        ),
    ],
)

ci.console_view(
    name = 'chromium.dawn',
    ordering = {
        None: ['ToT'],
        '*builder*': ['Builder'],
        '*cpu*': ci.ordering(short_names=['x86']),
        'ToT|Mac': '*builder*',
        'ToT|Windows|Builder': '*cpu*',
        'ToT|Windows|Intel': '*cpu*',
        'ToT|Windows|Nvidia': '*cpu*',
        'DEPS|Mac': '*builder*',
        'DEPS|Windows|Builder': '*cpu*',
        'DEPS|Windows|Intel': '*cpu*',
        'DEPS|Windows|Nvidia': '*cpu*',
    },
)

ci.console_view(
    name = 'chromium.fyi',
    ordering = {
        None: [
            'closure_compilation',
            'code_coverage',
            'cronet',
            'mac',
            'deterministic',
            'fuchsia',
            'chromeos',
            'iOS',
            'linux',
            'mojo',
            'recipe',
            'remote_run',
            'site_isolation',
            'network',
            'viz',
            'win10',
            'win32',
        ],
        'code_coverage': ci.ordering(
            short_names=['and', 'ann', 'lnx', 'lcr', 'mac']
        ),
        'mac': ci.ordering(short_names=['bld', '15', 'herm']),
        'deterministic|mac': ci.ordering(short_names=['rel', 'dbg']),
        'iOS|iOS13': ci.ordering(short_names=['dev', 'sim']),
        'linux|blink': ci.ordering(short_names=['TD']),
    },
)

ci.console_view(
    name ='chromium.fuzz',
    ordering = {
        None: [
            'afl',
            'win asan',
            'mac asan',
            'cros asan',
            'linux asan',
            'libfuzz',
            'linux msan',
            'linux tsan',
        ],
        '*config*': ci.ordering(short_names=['dbg', 'rel']),
        'win asan': '*config*',
        'mac asan': '*config*',
        'linux asan': '*config*',
        'linux asan|x64 v8-ARM': '*config*',
        'libfuzz': ci.ordering(short_names=[
            'chromeos-asan',
            'linux32',
            'linux32-dbg',
            'linux',
            'linux-dbg',
            'linux-msan',
            'linux-ubsan',
            'mac-asan',
            'win-asan',
        ]),
    },
)

ci.console_view(
    name = 'chromium.gpu',
    ordering = {
        None: ['Windows', 'Mac', 'Linux'],
    },
)

ci.console_view(
    name = 'chromium.gpu.fyi',
    ordering = {
        None: ['Windows', 'Mac', 'Linux'],
        '*builder*': ['Builder'],
        '*type*': ci.ordering(short_names=['rel', 'dbg', 'exp']),
        '*cpu*': ci.ordering(short_names=['x86']),
        'Windows': '*builder*',
        'Windows|Builder': ['Release', 'dEQP', 'dx12vk', 'Debug'],
        'Windows|Builder|Release': '*cpu*',
        'Windows|Builder|dEQP': '*cpu*',
        'Windows|Builder|dx12vk': '*type*',
        'Windows|Builder|Debug': '*cpu*',
        'Windows|10|x64|Intel': '*type*',
        'Windows|10|x64|Nvidia': '*type*',
        'Windows|10|x86|Nvidia': '*type*',
        'Windows|7|x64|Nvidia': '*type*',
        'Mac': '*builder*',
        'Mac|Builder': '*type*',
        'Mac|AMD|Retina': '*type*',
        'Mac|Intel': '*type*',
        'Mac|Nvidia': '*type*',
        'Linux': '*builder*',
        'Linux|Builder': '*type*',
        'Linux|Intel': '*type*',
        'Linux|Nvidia': '*type*',
        'Android': ['L32', 'M64', 'N64', 'P32', 'vk', 'dqp', 'skgl', 'skv'],
        'Android|M64': ['QCOM'],
    },
)

ci.console_view(
    name = 'chromium.linux',
    ordering = {
        None: ['release', 'debug'],
        'release': ci.ordering(short_names=['bld', 'tst', 'nsl', 'gcc']),
        'cast': ci.ordering(short_names=['vid', 'aud']),
    },
)

ci.console_view(
    name = 'chromium.mac',
    ordering = {
        None: ['release'],
        'release': ci.ordering(short_names=['bld']),
        'debug': ci.ordering(short_names=['bld']),
        'ios|default': ci.ordering(short_names=['dev', 'sim']),
    },
)

ci.console_view(
    name = 'chromium.memory',
    ordering = {
        None: ['win', 'mac', 'linux', 'cros'],
        '*build-or-test*': ci.ordering(short_names=['bld', 'tst']),
        'linux|TSan v2': '*build-or-test*',
        'linux|asan lsan': '*build-or-test*',
        'linux|webkit': ci.ordering(short_names=['asn', 'msn']),
    },
)

ci.console_view(
    name = 'chromium.swangle',
    ordering = {
        None: ['DEPS', 'ToT ANGLE', 'ToT SwiftShader'],
        '*os*': ['Windows', 'Mac'],
        '*cpu*': ci.ordering(short_names=['x86', 'x64']),
        'DEPS': '*os*',
        'DEPS|Windows': '*cpu*',
        'DEPS|Linux': '*cpu*',
        'ToT ANGLE': '*os*',
        'ToT ANGLE|Windows': '*cpu*',
        'ToT ANGLE|Linux': '*cpu*',
        'ToT SwiftShader': '*os*',
        'ToT SwiftShader|Windows': '*cpu*',
        'ToT SwiftShader|Linux': '*cpu*',
        'Chromium': '*os*',
    },
)

ci.console_view(
    name = 'chromium.win',
    ordering = {
        None: ['release', 'debug'],
        'debug|builder': ci.ordering(short_names=['64', '32']),
        'debug|tester': ci.ordering(short_names=['7', '10']),
    },
)

# The main console includes some entries for builders from the chrome project
[luci.console_view_entry(
    builder = 'chrome:ci/{}'.format(name),
    console_view = 'main',
    category = 'chrome',
    short_name = short_name,
) for name, short_name in (
    ('linux-chromeos-chrome', 'cro'),
    ('linux-chrome', 'lnx'),
    ('mac-chrome', 'mac'),
    ('win-chrome', 'win'),
    ('win64-chrome', 'win'),
)]


# Builders are sorted first lexicographically by the function used to define
# them, then lexicographically by their name


ci.builder(
    name = 'android-avd-packager',
    executable = 'recipe:android/avd_packager',
    properties = {
        'avd_configs': [
            'tools/android/avd/proto/creation/generic_android23.textpb',
            'tools/android/avd/proto/creation/generic_android28.textpb',
            'tools/android/avd/proto/creation/generic_playstore_android28.textpb',
        ],
    },
    schedule = '0 7 * * 0 *',
    service_account = 'chromium-cipd-builder@chops-service-accounts.iam.gserviceaccount.com',
    triggered_by = [],
)

ci.builder(
    name = 'android-sdk-packager',
    executable = 'recipe:android/sdk_packager',
    schedule = '0 7 * * 0 *',
    service_account = 'chromium-cipd-builder@chops-service-accounts.iam.gserviceaccount.com',
    triggered_by = [],
    properties = {
        # We still package part of build-tools;25.0.2 to support
        # http://bit.ly/2KNUygZ
        'packages': [
            {
                'sdk_package_name': 'build-tools;25.0.2',
                'cipd_yaml': 'third_party/android_sdk/cipd/build-tools/25.0.2.yaml'
            },
            {
                'sdk_package_name': 'build-tools;27.0.3',
                'cipd_yaml': 'third_party/android_sdk/cipd/build-tools/27.0.3.yaml'
            },
            {
                'sdk_package_name': 'build-tools;29.0.2',
                'cipd_yaml': 'third_party/android_sdk/cipd/build-tools/29.0.2.yaml'
            },
            {
                'sdk_package_name': 'emulator',
                'cipd_yaml': 'third_party/android_sdk/cipd/emulator.yaml'
            },
            {
                'sdk_package_name': 'extras;google;gcm',
                'cipd_yaml': 'third_party/android_sdk/cipd/extras/google/gcm.yaml'
            },
            {
                'sdk_package_name': 'patcher;v4',
                'cipd_yaml': 'third_party/android_sdk/cipd/patcher/v4.yaml'
            },
            {
                'sdk_package_name': 'platforms;android-23',
                'cipd_yaml': 'third_party/android_sdk/cipd/platforms/android-23.yaml'
            },
            {
                'sdk_package_name': 'platforms;android-28',
                'cipd_yaml': 'third_party/android_sdk/cipd/platforms/android-28.yaml'
            },
            {
                'sdk_package_name': 'platforms;android-29',
                'cipd_yaml': 'third_party/android_sdk/cipd/platforms/android-29.yaml'
            },
            {
                'sdk_package_name': 'platform-tools',
                'cipd_yaml': 'third_party/android_sdk/cipd/platform-tools.yaml'
            },
            {
                'sdk_package_name': 'sources;android-28',
                'cipd_yaml': 'third_party/android_sdk/cipd/sources/android-28.yaml'
            },
            {
                'sdk_package_name': 'sources;android-29',
                'cipd_yaml': 'third_party/android_sdk/cipd/sources/android-29.yaml'
            },
            {
                'sdk_package_name': 'system-images;android-23;google_apis;x86',
                'cipd_yaml': 'third_party/android_sdk/cipd/system_images/android-23/google_apis/x86.yaml'
            },
            {
                'sdk_package_name': 'system-images;android-28;google_apis;x86',
                'cipd_yaml': 'third_party/android_sdk/cipd/system_images/android-28/google_apis/x86.yaml'
            },
            {
                'sdk_package_name': 'system-images;android-28;google_apis_playstore;x86',
                'cipd_yaml': 'third_party/android_sdk/cipd/system_images/android-28/google_apis_playstore/x86.yaml'
            },
            {
                'sdk_package_name': 'system-images;android-29;google_apis;x86',
                'cipd_yaml': 'third_party/android_sdk/cipd/system_images/android-29/google_apis/x86.yaml'
            },
            {
                'sdk_package_name': 'system-images;android-29;google_apis_playstore;x86',
                'cipd_yaml': 'third_party/android_sdk/cipd/system_images/android-29/google_apis_playstore/x86.yaml'
            },
        ],
    },
)


ci.android_builder(
    name = 'Android ASAN (dbg)',
    console_view_entry = ci.console_view_entry(
        category = 'builder|arm',
        short_name = 'san',
    ),
    # Higher build timeout since dbg ASAN builds can take a while on a clobber
    # build.
    execution_timeout = 4 * time.hour,
)

ci.android_builder(
    name = 'Android WebView L (dbg)',
    console_view_entry = ci.console_view_entry(
        category = 'tester|webview',
        short_name = 'L',
    ),
    main_console_view = 'main',
    triggered_by = ['ci/Android arm Builder (dbg)'],
)

ci.android_builder(
    name = 'Deterministic Android',
    console_view_entry = ci.console_view_entry(
        category = 'builder|det',
        short_name = 'rel',
    ),
    executable = 'recipe:swarming/deterministic_build',
    execution_timeout = 6 * time.hour,
)

ci.android_builder(
    name = 'Deterministic Android (dbg)',
    console_view_entry = ci.console_view_entry(
        category = 'builder|det',
        short_name = 'dbg',
    ),
    executable = 'recipe:swarming/deterministic_build',
    execution_timeout = 6 * time.hour,
)

ci.android_builder(
    name = 'Lollipop Phone Tester',
    console_view_entry = ci.console_view_entry(
        category = 'tester|phone',
        short_name = 'L',
    ),
    # We have limited phone capacity and thus limited ability to run
    # tests in parallel, hence the high timeout.
    execution_timeout = 6 * time.hour,
    main_console_view = 'main',
    triggered_by = ['ci/Android arm Builder (dbg)'],
)

ci.android_builder(
    name = 'Lollipop Tablet Tester',
    console_view_entry = ci.console_view_entry(
        category = 'tester|tablet',
        short_name = 'L',
    ),
    # We have limited tablet capacity and thus limited ability to run
    # tests in parallel, hence the high timeout.
    execution_timeout = 20 * time.hour,
    main_console_view = 'main',
    triggered_by = ['ci/Android arm Builder (dbg)'],
)

ci.android_builder(
    name = 'Marshmallow Tablet Tester',
    console_view_entry = ci.console_view_entry(
        category = 'tester|tablet',
        short_name = 'M',
    ),
    # We have limited tablet capacity and thus limited ability to run
    # tests in parallel, hence the high timeout.
    execution_timeout = 12 * time.hour,
    main_console_view = 'main',
    triggered_by = ['ci/Android arm Builder (dbg)'],
)

ci.android_builder(
    name = 'android-arm64-proguard-rel',
    console_view_entry = ci.console_view_entry(
        category = 'builder_tester|arm64',
        short_name = 'M proguard',
    ),
    goma_jobs = goma.jobs.MANY_JOBS_FOR_CI,
    execution_timeout = 6 * time.hour,
)

ci.android_builder(
    name = 'android-cronet-arm64-dbg',
    console_view_entry = ci.console_view_entry(
        category = 'cronet|arm64',
        short_name = 'dbg',
    ),
    notifies = ['cronet'],
)

ci.android_builder(
    name = 'android-cronet-arm64-rel',
    console_view_entry = ci.console_view_entry(
        category = 'cronet|arm64',
        short_name = 'rel',
    ),
    notifies = ['cronet'],
)

ci.android_builder(
    name = 'android-cronet-asan-arm-rel',
    console_view_entry = ci.console_view_entry(
        category = 'cronet|asan',
    ),
    notifies = ['cronet'],
)

# Runs on a specific machine with an attached phone
ci.android_builder(
    name = 'android-cronet-marshmallow-arm64-perf-rel',
    console_view_entry = ci.console_view_entry(
        category = 'cronet|test|perf',
        short_name = 'm',
    ),
    cores = None,
    cpu = None,
    executable = 'recipe:cronet',
    notifies = ['cronet'],
    os = os.ANDROID,
)

ci.android_builder(
    name = 'android-cronet-marshmallow-arm64-rel',
    console_view_entry = ci.console_view_entry(
        category = 'cronet|test',
        short_name = 'm',
    ),
    notifies = ['cronet'],
    triggered_by = ['android-cronet-arm64-rel'],
)

ci.android_builder(
    name = 'android-cronet-x86-dbg',
    console_view_entry = ci.console_view_entry(
        category = 'cronet|x86',
        short_name = 'dbg',
    ),
    notifies = ['cronet'],
)

ci.android_builder(
    name = 'android-cronet-x86-rel',
    console_view_entry = ci.console_view_entry(
        category = 'cronet|x86',
        short_name = 'rel',
    ),
    notifies = ['cronet'],
)

ci.android_builder(
    name = 'android-incremental-dbg',
    console_view_entry = ci.console_view_entry(
        category = 'tester|incremental',
    ),
)

ci.android_builder(
    name = 'android-pie-x86-rel',
    console_view_entry = ci.console_view_entry(
        category = 'builder_tester|x86',
        short_name = 'P',
    ),
)

ci.android_builder(
    name = 'android-10-arm64-rel',
    console_view_entry = ci.console_view_entry(
        category = 'builder_tester|arm64',
        short_name = '10',
    ),
)


ci.android_fyi_builder(
    name = 'android-bfcache-rel',
    console_view_entry = ci.console_view_entry(
        category = 'android',
    ),
)

ci.android_fyi_builder(
    name = 'Android WebLayer P FYI (rel)',
    console_view_entry = ci.console_view_entry(
        category = 'weblayer',
        short_name = 'p-rel',
    ),
)

ci.android_fyi_builder(
    name = 'android-weblayer-pie-x86-fyi-rel',
    console_view_entry = ci.console_view_entry(
        category = 'weblayer',
        short_name = 'p-x86-rel',
    ),
)

ci.android_fyi_builder(
    name = 'Android WebView P Blink-CORS FYI (rel)',
    console_view_entry = ci.console_view_entry(
        category = 'webview',
        short_name = 'cors',
    ),
)

ci.android_fyi_builder(
    name = 'Android WebView P FYI (rel)',
    console_view_entry = ci.console_view_entry(
        category = 'webview',
        short_name = 'p-rel',
    ),
)

ci.android_fyi_builder(
    name = 'android-marshmallow-x86-fyi-rel',
    console_view_entry = ci.console_view_entry(
        category = 'emulator|M|x86',
        short_name = 'rel',
    ),
    schedule = '0 7 * * *',
    triggered_by = [],
)

# TODO(hypan): remove this once there is no associated disabled tests
ci.android_fyi_builder(
    name = 'android-pie-x86-fyi-rel',
    console_view_entry = ci.console_view_entry(
        category = 'emulator|P|x86',
        short_name = 'rel',
    ),
    goma_jobs=goma.jobs.J150,
    schedule = 'triggered',  # triggered manually via Scheduler UI
)



ci.chromium_builder(
    name = 'android-archive-dbg',
    # Bump to 32 if needed.
    console_view_entry = ci.console_view_entry(
        category = 'android',
        short_name = 'dbg',
    ),
    cores = 8,
    main_console_view = 'main',
)

ci.chromium_builder(
    name = 'android-archive-rel',
    console_view_entry = ci.console_view_entry(
        category = 'android',
        short_name = 'rel',
    ),
    cores = 32,
    main_console_view = 'main',
)

ci.chromium_builder(
    name = 'linux-archive-dbg',
    console_view_entry = ci.console_view_entry(
        category = 'linux',
        short_name = 'dbg',
    ),
    # Bump to 32 if needed.
    cores = 8,
    main_console_view = 'main',
)

ci.chromium_builder(
    name = 'linux-archive-rel',
    console_view_entry = ci.console_view_entry(
        category = 'linux',
        short_name = 'rel',
    ),
    cores = 32,
    main_console_view = 'main',
)

ci.chromium_builder(
    name = 'mac-archive-dbg',
    console_view_entry = ci.console_view_entry(
        category = 'mac',
        short_name = 'dbg',
    ),
    # Bump to 8 cores if needed.
    cores = 4,
    main_console_view = 'main',
    os = os.MAC_DEFAULT,
)

ci.chromium_builder(
    name = 'mac-archive-rel',
    console_view_entry = ci.console_view_entry(
        category = 'mac',
        short_name = 'rel',
    ),
    main_console_view = 'main',
    os = os.MAC_DEFAULT,
)

ci.chromium_builder(
    name = 'win-archive-dbg',
    console_view_entry = ci.console_view_entry(
        category = 'win|dbg',
        short_name = '64',
    ),
    cores = 32,
    main_console_view = 'main',
    os = os.WINDOWS_DEFAULT,
)

ci.chromium_builder(
    name = 'win-archive-rel',
    console_view_entry = ci.console_view_entry(
        category = 'win|rel',
        short_name = '64',
    ),
    cores = 32,
    main_console_view = 'main',
    os = os.WINDOWS_DEFAULT,
)

ci.chromium_builder(
    name = 'win32-archive-dbg',
    console_view_entry = ci.console_view_entry(
        category = 'win|dbg',
        short_name = '32',
    ),
    cores = 32,
    main_console_view = 'main',
    os = os.WINDOWS_DEFAULT,
)

ci.chromium_builder(
    name = 'win32-archive-rel',
    console_view_entry = ci.console_view_entry(
        category = 'win|rel',
        short_name = '32',
    ),
    cores = 32,
    main_console_view = 'main',
    os = os.WINDOWS_DEFAULT,
)


ci.chromiumos_builder(
    name = 'Linux ChromiumOS Full',
    console_view_entry = ci.console_view_entry(
        category = 'default',
        short_name = 'ful',
    ),
)

ci.chromiumos_builder(
    name = 'chromeos-amd64-generic-asan-rel',
    console_view_entry = ci.console_view_entry(
        category = 'simple|release|x64',
        short_name = 'asn',
    ),
)

ci.chromiumos_builder(
    name = 'chromeos-amd64-generic-cfi-thin-lto-rel',
    console_view_entry = ci.console_view_entry(
        category = 'simple|release|x64',
        short_name = 'cfi',
    ),
)

ci.chromiumos_builder(
    name = 'chromeos-arm-generic-dbg',
    console_view_entry = ci.console_view_entry(
        category = 'simple|debug',
        short_name = 'arm',
    ),
)


ci.clang_builder(
    name = 'CFI Linux CF',
    goma_backend = goma.backend.RBE_PROD,
    console_view_entry = ci.console_view_entry(
        category = 'CFI|Linux',
        short_name = 'CF',
    ),
)

ci.clang_builder(
    name = 'CFI Linux ToT',
    console_view_entry = ci.console_view_entry(
        category = 'CFI|Linux',
        short_name = 'ToT',
    ),
)

ci.clang_builder(
    name = 'CrWinAsan',
    console_view_entry = ci.console_view_entry(
        category = 'ToT Windows|Asan',
        short_name = 'asn',
    ),
    os = os.WINDOWS_ANY,
)

ci.clang_builder(
    name = 'CrWinAsan(dll)',
    console_view_entry = ci.console_view_entry(
        category = 'ToT Windows|Asan',
        short_name = 'dll',
    ),
    os = os.WINDOWS_ANY,
)

ci.clang_builder(
    name = 'ToTAndroid',
    console_view_entry = ci.console_view_entry(
        category = 'ToT Android',
        short_name = 'rel',
    ),
)

ci.clang_builder(
    name = 'ToTAndroid (dbg)',
    console_view_entry = ci.console_view_entry(
        category = 'ToT Android',
        short_name = 'dbg',
    ),
)

ci.clang_builder(
    name = 'ToTAndroid x64',
    console_view_entry = ci.console_view_entry(
        category = 'ToT Android',
        short_name = 'x64',
    ),
)

ci.clang_builder(
    name = 'ToTAndroid64',
    console_view_entry = ci.console_view_entry(
        category = 'ToT Android',
        short_name = 'a64',
    ),
)

ci.clang_builder(
    name = 'ToTAndroidASan',
    console_view_entry = ci.console_view_entry(
        category = 'ToT Android',
        short_name = 'asn',
    ),
)

ci.clang_builder(
    name = 'ToTAndroidCFI',
    console_view_entry = ci.console_view_entry(
        category = 'ToT Android',
        short_name = 'cfi',
    ),
)

ci.clang_builder(
    name = 'ToTAndroidOfficial',
    console_view_entry = ci.console_view_entry(
        category = 'ToT Android',
        short_name = 'off',
    ),
)

ci.clang_builder(
    name = 'ToTLinux',
    console_view_entry = ci.console_view_entry(
        category = 'ToT Linux',
        short_name = 'rel',
    ),
)

ci.clang_builder(
    name = 'ToTLinux (dbg)',
    console_view_entry = ci.console_view_entry(
        category = 'ToT Linux',
        short_name = 'dbg',
    ),
)

ci.clang_builder(
    name = 'ToTLinuxASan',
    console_view_entry = ci.console_view_entry(
        category = 'ToT Linux',
        short_name = 'asn',
    ),
)

ci.clang_builder(
    name = 'ToTLinuxASanLibfuzzer',
    console_view_entry = ci.console_view_entry(
        category = 'ToT Linux',
        short_name = 'fuz',
    ),
)

ci.clang_builder(
    name = 'ToTLinuxCoverage',
    console_view_entry = ci.console_view_entry(
        category = 'ToT Code Coverage',
        short_name = 'linux',
    ),
    executable = 'recipe:chromium_clang_coverage_tot',
)

ci.clang_builder(
    name = 'ToTLinuxMSan',
    console_view_entry = ci.console_view_entry(
        category = 'ToT Linux',
        short_name = 'msn',
    ),
)

ci.clang_builder(
    name = 'ToTLinuxTSan',
    console_view_entry = ci.console_view_entry(
        category = 'ToT Linux',
        short_name = 'tsn',
    ),
)

ci.clang_builder(
    name = 'ToTLinuxThinLTO',
    console_view_entry = ci.console_view_entry(
        category = 'ToT Linux',
        short_name = 'lto',
    ),
)

ci.clang_builder(
    name = 'ToTLinuxUBSanVptr',
    console_view_entry = ci.console_view_entry(
        category = 'ToT Linux',
        short_name = 'usn',
    ),
)

ci.clang_builder(
    name = 'ToTWin(dbg)',
    console_view_entry = ci.console_view_entry(
        category = 'ToT Windows',
        short_name = 'dbg',
    ),
    os = os.WINDOWS_ANY,
)

ci.clang_builder(
    name = 'ToTWin(dll)',
    console_view_entry = ci.console_view_entry(
        category = 'ToT Windows',
        short_name = 'dll',
    ),
    os = os.WINDOWS_ANY,
)

ci.clang_builder(
    name = 'ToTWin64(dbg)',
    console_view_entry = ci.console_view_entry(
        category = 'ToT Windows|x64',
        short_name = 'dbg',
    ),
    os = os.WINDOWS_ANY,
)

ci.clang_builder(
    name = 'ToTWin64(dll)',
    console_view_entry = ci.console_view_entry(
        category = 'ToT Windows|x64',
        short_name = 'dll',
    ),
    os = os.WINDOWS_ANY,
)

ci.clang_builder(
    name = 'ToTWinASanLibfuzzer',
    console_view_entry = ci.console_view_entry(
        category = 'ToT Windows|Asan',
        short_name = 'fuz',
    ),
    os = os.WINDOWS_ANY,
)

ci.clang_builder(
    name = 'ToTWinCFI',
    console_view_entry = ci.console_view_entry(
        category = 'CFI|Win',
        short_name = 'x86',
    ),
    os = os.WINDOWS_ANY,
)

ci.clang_builder(
    name = 'ToTWinCFI64',
    console_view_entry = ci.console_view_entry(
        category = 'CFI|Win',
        short_name = 'x64',
    ),
    os = os.WINDOWS_ANY,
)

ci.clang_builder(
    name = 'UBSanVptr Linux',
    console_view_entry = ci.console_view_entry(
        short_name = 'usn',
    ),
    goma_backend = goma.backend.RBE_PROD,
)

ci.clang_builder(
    name = 'linux-win_cross-rel',
    console_view_entry = ci.console_view_entry(
        category = 'ToT Windows',
        short_name = 'lxw',
    ),
)

ci.clang_builder(
    name = 'ToTiOS',
    caches = [xcode_cache.x11c29],
    console_view_entry = ci.console_view_entry(
        category = 'iOS|public',
        short_name = 'sim',
    ),
    cores = None,
    os = os.MAC_10_14,
    properties = {
        'xcode_build_version': '11c29'
    },
    ssd=True
)

ci.clang_builder(
    name = 'ToTiOSDevice',
    caches = [xcode_cache.x11c29],
    console_view_entry = ci.console_view_entry(
        category = 'iOS|public',
        short_name = 'dev',
    ),
    cores = None,
    os = os.MAC_10_14,
    properties = {
        'xcode_build_version': '11c29'
    },
    ssd=True
)


ci.clang_mac_builder(
    name = 'ToTMac',
    console_view_entry = ci.console_view_entry(
        category = 'ToT Mac',
        short_name = 'rel',
    ),
)

ci.clang_mac_builder(
    name = 'ToTMac (dbg)',
    console_view_entry = ci.console_view_entry(
        category = 'ToT Mac',
        short_name = 'dbg',
    ),
)

ci.clang_mac_builder(
    name = 'ToTMacASan',
    console_view_entry = ci.console_view_entry(
        category = 'ToT Mac',
        short_name = 'asn',
    ),
)

ci.clang_mac_builder(
    name = 'ToTMacCoverage',
    console_view_entry = ci.console_view_entry(
        category = 'ToT Code Coverage',
        short_name = 'mac',
    ),
    executable = 'recipe:chromium_clang_coverage_tot',
)


ci.dawn_builder(
    name = 'Dawn Linux x64 Builder',
    console_view_entry = ci.console_view_entry(
        category = 'ToT|Linux|Builder',
        short_name = 'x64',
    ),
)

ci.dawn_builder(
    name = 'Dawn Linux x64 Release (Intel HD 630)',
    console_view_entry = ci.console_view_entry(
        category = 'ToT|Linux|Intel',
        short_name = 'x64',
    ),
    cores = 2,
    os = os.LINUX_DEFAULT,
    triggered_by = ['Dawn Linux x64 Builder'],
)

ci.dawn_builder(
    name = 'Dawn Linux x64 Release (NVIDIA)',
    console_view_entry = ci.console_view_entry(
        category = 'ToT|Linux|Nvidia',
        short_name = 'x64',
    ),
    cores = 2,
    os = os.LINUX_DEFAULT,
    triggered_by = ['Dawn Linux x64 Builder'],
)

ci.dawn_builder(
    name = 'Dawn Mac x64 Builder',
    console_view_entry = ci.console_view_entry(
        category = 'ToT|Mac|Builder',
        short_name = 'x64',
    ),
    builderless = False,
    cores = None,
    os = os.MAC_ANY,
)

ci.dawn_builder(
    name = 'Dawn Mac x64 Release (AMD)',
    console_view_entry = ci.console_view_entry(
        category = 'ToT|Mac|AMD',
        short_name = 'x64',
    ),
    cores = 2,
    os = os.LINUX_DEFAULT,
    triggered_by = ['Dawn Mac x64 Builder'],
)

ci.dawn_builder(
    name = 'Dawn Mac x64 Release (Intel)',
    console_view_entry = ci.console_view_entry(
        category = 'ToT|Mac|Intel',
        short_name = 'x64',
    ),
    cores = 2,
    os = os.LINUX_DEFAULT,
    triggered_by = ['Dawn Mac x64 Builder'],
)

ci.dawn_builder(
    name = 'Dawn Win10 x86 Builder',
    console_view_entry = ci.console_view_entry(
        category = 'ToT|Windows|Builder',
        short_name = 'x86',
    ),
    os = os.WINDOWS_ANY,
)

ci.dawn_builder(
    name = 'Dawn Win10 x64 Builder',
    console_view_entry = ci.console_view_entry(
        category = 'ToT|Windows|Builder',
        short_name = 'x64',
    ),
    os = os.WINDOWS_ANY,
)

# Note that the Win testers are all thin Linux VMs, triggering jobs on the
# physical Win hardware in the Swarming pool, which is why they run on linux
ci.dawn_builder(
    name = 'Dawn Win10 x86 Release (Intel HD 630)',
    console_view_entry = ci.console_view_entry(
        category = 'ToT|Windows|Intel',
        short_name = 'x86',
    ),
    cores = 2,
    os = os.LINUX_DEFAULT,
    triggered_by = ['Dawn Win10 x86 Builder'],
)

ci.dawn_builder(
    name = 'Dawn Win10 x64 Release (Intel HD 630)',
    console_view_entry = ci.console_view_entry(
        category = 'ToT|Windows|Intel',
        short_name = 'x64',
    ),
    cores = 2,
    os = os.LINUX_DEFAULT,
    triggered_by = ['Dawn Win10 x64 Builder'],
)

ci.dawn_builder(
    name = 'Dawn Win10 x86 Release (NVIDIA)',
    console_view_entry = ci.console_view_entry(
        category = 'ToT|Windows|Nvidia',
        short_name = 'x86',
    ),
    cores = 2,
    os = os.LINUX_DEFAULT,
    triggered_by = ['Dawn Win10 x86 Builder'],
)

ci.dawn_builder(
    name = 'Dawn Win10 x64 Release (NVIDIA)',
    console_view_entry = ci.console_view_entry(
        category = 'ToT|Windows|Nvidia',
        short_name = 'x64',
    ),
    cores = 2,
    os = os.LINUX_DEFAULT,
    triggered_by = ['Dawn Win10 x64 Builder'],
)


ci.fuzz_builder(
    name = 'ASAN Debug',
    console_view_entry = ci.console_view_entry(
        category = 'linux asan',
        short_name = 'dbg',
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.fuzz_builder(
    name = 'ASan Debug (32-bit x86 with V8-ARM)',
    console_view_entry = ci.console_view_entry(
        category = 'linux asan|x64 v8-ARM',
        short_name = 'dbg',
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.fuzz_builder(
    name = 'ASAN Release',
    console_view_entry = ci.console_view_entry(
        category = 'linux asan',
        short_name = 'rel',
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 5,
    ),
)

ci.fuzz_builder(
    name = 'ASan Release (32-bit x86 with V8-ARM)',
    console_view_entry = ci.console_view_entry(
        category = 'linux asan|x64 v8-ARM',
        short_name = 'rel',
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.fuzz_builder(
    name = 'ASAN Release Media',
    console_view_entry = ci.console_view_entry(
        category = 'linux asan',
        short_name = 'med',
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.fuzz_builder(
    name = 'Afl Upload Linux ASan',
    console_view_entry = ci.console_view_entry(
        category = 'afl',
        short_name = 'afl',
    ),
    executable = 'recipe:chromium_afl',
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.fuzz_builder(
    name = 'ASan Release Media (32-bit x86 with V8-ARM)',
    console_view_entry = ci.console_view_entry(
        category = 'linux asan|x64 v8-ARM',
        short_name = 'med',
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.fuzz_builder(
    name = 'ChromiumOS ASAN Release',
    console_view_entry = ci.console_view_entry(
        category = 'cros asan',
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 6,
    ),
)

ci.fuzz_builder(
    name = 'MSAN Release (chained origins)',
    console_view_entry = ci.console_view_entry(
        category = 'linux msan',
        short_name = 'org',
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.fuzz_builder(
    name = 'MSAN Release (no origins)',
    console_view_entry = ci.console_view_entry(
        category = 'linux msan',
        short_name = 'rel',
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.fuzz_builder(
    name = 'Mac ASAN Release',
    builderless = False,
    console_view_entry = ci.console_view_entry(
        category = 'mac asan',
        short_name = 'rel',
    ),
    cores = 4,
    os = os.MAC_DEFAULT,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 2,
    ),
)

ci.fuzz_builder(
    name = 'Mac ASAN Release Media',
    builderless = False,
    console_view_entry = ci.console_view_entry(
        category = 'mac asan',
        short_name = 'med',
    ),
    cores = 4,
    os = os.MAC_DEFAULT,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 2,
    ),
)

ci.fuzz_builder(
    name = 'TSAN Debug',
    console_view_entry = ci.console_view_entry(
        category = 'linux tsan',
        short_name = 'dbg',
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.fuzz_builder(
    name = 'TSAN Release',
    console_view_entry = ci.console_view_entry(
        category = 'linux tsan',
        short_name = 'rel',
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 3,
    ),
)

ci.fuzz_builder(
    name = 'UBSan Release',
    console_view_entry = ci.console_view_entry(
        category = 'linux UBSan',
        short_name = 'rel',
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.fuzz_builder(
    name = 'UBSan vptr Release',
    console_view_entry = ci.console_view_entry(
        category = 'linux UBSan',
        short_name = 'vpt',
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.fuzz_builder(
    name = 'Win ASan Release',
    builderless = False,
    console_view_entry = ci.console_view_entry(
        category = 'win asan',
        short_name = 'rel',
    ),
    os = os.WINDOWS_DEFAULT,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 7,
    ),
)

ci.fuzz_builder(
    name = 'Win ASan Release Media',
    builderless = False,
    console_view_entry = ci.console_view_entry(
        category = 'win asan',
        short_name = 'med',
    ),
    os = os.WINDOWS_DEFAULT,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 6,
    ),
)


ci.fuzz_libfuzzer_builder(
    name = 'Libfuzzer Upload Chrome OS ASan',
    console_view_entry = ci.console_view_entry(
        category = 'libfuzz',
        short_name = 'chromeos-asan',
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 3,
    ),
)

ci.fuzz_libfuzzer_builder(
    name = 'Libfuzzer Upload Linux ASan',
    console_view_entry = ci.console_view_entry(
        category = 'libfuzz',
        short_name = 'linux',
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 5,
    ),
)

ci.fuzz_libfuzzer_builder(
    name = 'Libfuzzer Upload Linux ASan Debug',
    console_view_entry = ci.console_view_entry(
        category = 'libfuzz',
        short_name = 'linux-dbg',
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 5,
    ),
)

ci.fuzz_libfuzzer_builder(
    name = 'Libfuzzer Upload Linux MSan',
    console_view_entry = ci.console_view_entry(
        category = 'libfuzz',
        short_name = 'linux-msan',
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 5,
    ),
)

ci.fuzz_libfuzzer_builder(
    name = 'Libfuzzer Upload Linux UBSan',
    # Do not use builderless for this (crbug.com/980080).
    builderless = False,
    console_view_entry = ci.console_view_entry(
        category = 'libfuzz',
        short_name = 'linux-ubsan',
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 5,
    ),
)

ci.fuzz_libfuzzer_builder(
    name = 'Libfuzzer Upload Linux V8-ARM64 ASan',
    console_view_entry = ci.console_view_entry(
        category = 'libfuzz',
        short_name = 'arm64',
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 1,
    ),
)

ci.fuzz_libfuzzer_builder(
    name = 'Libfuzzer Upload Linux V8-ARM64 ASan Debug',
    console_view_entry = ci.console_view_entry(
        category = 'libfuzz',
        short_name = 'arm64-dbg',
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 1,
    ),
)

ci.fuzz_libfuzzer_builder(
    name = 'Libfuzzer Upload Linux32 ASan',
    console_view_entry = ci.console_view_entry(
        category = 'libfuzz',
        short_name = 'linux32',
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 3,
    ),
)

ci.fuzz_libfuzzer_builder(
    name = 'Libfuzzer Upload Linux32 ASan Debug',
    console_view_entry = ci.console_view_entry(
        category = 'libfuzz',
        short_name = 'linux32-dbg',
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 3,
    ),
)

ci.fuzz_libfuzzer_builder(
    name = 'Libfuzzer Upload Linux32 V8-ARM ASan',
    console_view_entry = ci.console_view_entry(
        category = 'libfuzz',
        short_name = 'arm',
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 1,
    ),
)

ci.fuzz_libfuzzer_builder(
    name = 'Libfuzzer Upload Linux32 V8-ARM ASan Debug',
    console_view_entry = ci.console_view_entry(
        category = 'libfuzz',
        short_name = 'arm-dbg',
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 1,
    ),
)

ci.fuzz_libfuzzer_builder(
    name = 'Libfuzzer Upload Mac ASan',
    console_view_entry = ci.console_view_entry(
        category = 'libfuzz',
        short_name = 'mac-asan',
    ),
    cores = 24,
    execution_timeout = 4 * time.hour,
    os = os.MAC_DEFAULT,
)

ci.fuzz_libfuzzer_builder(
    name = 'Libfuzzer Upload Windows ASan',
    console_view_entry = ci.console_view_entry(
        category = 'libfuzz',
        short_name = 'win-asan',
    ),
    os = os.WINDOWS_DEFAULT,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 3,
    ),
)


ci.fyi_builder(
    name = 'Closure Compilation Linux',
    console_view_entry = ci.console_view_entry(
        category = 'closure_compilation',
    ),
    executable = 'recipe:closure_compilation',
)

ci.fyi_builder(
    name = 'Linux Viz',
    console_view_entry = ci.console_view_entry(
        category = 'viz',
    ),
)

ci.fyi_builder(
    name = 'Linux remote_run Builder',
    console_view_entry = ci.console_view_entry(
        category = 'remote_run',
    ),
)

ci.fyi_builder(
    name = 'Linux remote_run Tester',
    console_view_entry = ci.console_view_entry(
        category = 'remote_run',
    ),
    triggered_by = ['Linux remote_run Builder'],
)

ci.fyi_builder(
    name = 'Mojo Android',
    console_view_entry = ci.console_view_entry(
        category = 'mojo',
        short_name = 'and',
    ),
)

ci.fyi_builder(
    name = 'Mojo ChromiumOS',
    console_view_entry = ci.console_view_entry(
        category = 'mojo',
        short_name = 'cr',
    ),
)

ci.fyi_builder(
    name = 'Mojo Linux',
    console_view_entry = ci.console_view_entry(
        category = 'mojo',
        short_name = 'lnx',
    ),
)

ci.fyi_builder(
    name = 'Site Isolation Android',
    console_view_entry = ci.console_view_entry(
        category = 'site_isolation',
    ),
)

ci.fyi_builder(
    name = 'android-mojo-webview-rel',
    console_view_entry = ci.console_view_entry(
        category = 'mojo',
        short_name = 'aw',
    ),
)

ci.fyi_builder(
    name = 'chromeos-amd64-generic-lacros-rel',
    console_view_entry = ci.console_view_entry(
        category = 'chromeos',
    ),
    properties = {
        # The format of these properties is defined at archive/properties.proto
        '$build/archive' : {
            'archive_datas': [
            {
                'files': [
                    'chrome',
                    'chrome_100_percent.pak',
                    'chrome_200_percent.pak',
                    'crashpad_handler',
                    'headless_lib.pak',
                    'icudtl.dat',
                    'nacl_helper',
                    'nacl_irt_x86_64.nexe',
                    'resources.pak',
                    'snapshot_blob.bin',
                ],
                'dirs': ['locales', 'swiftshader'],
                'gcs_bucket': 'chromium-lacros-fishfood',
                'gcs_path': 'x86_64/{%position%}/lacros.zip',
                'archive_type': 'ARCHIVE_TYPE_ZIP',
            },
        ],
      },
    },
)

ci.fyi_builder(
    name = 'chromeos-amd64-generic-rel-vm-tests',
    console_view_entry = ci.console_view_entry(
        category = 'chromeos',
    ),
)

ci.fyi_builder(
    name = 'fuchsia-fyi-arm64-rel',
    console_view_entry = ci.console_view_entry(
        category = 'fuchsia|a64',
        short_name = 'rel',
    ),
    notifies = ['cr-fuchsia'],
)

ci.fyi_builder(
    name = 'fuchsia-fyi-x64-dbg',
    console_view_entry = ci.console_view_entry(
        category = 'fuchsia|x64',
        short_name = 'dbg',
    ),
    notifies = ['cr-fuchsia'],
)

ci.fyi_builder(
    name = 'fuchsia-fyi-x64-rel',
    console_view_entry = ci.console_view_entry(
        category = 'fuchsia|x64',
        short_name = 'rel',
    ),
    notifies = ['cr-fuchsia'],
)

ci.fyi_builder(
    name = 'linux-annotator-rel',
    console_view_entry = ci.console_view_entry(
        category = 'network|traffic|annotations',
        short_name = 'lnx',
    ),
)

ci.fyi_builder(
    name = 'linux-bfcache-rel',
    console_view_entry = ci.console_view_entry(
        category = 'linux',
    ),
)

ci.fyi_builder(
    name = 'linux-blink-animation-use-time-delta',
    console_view_entry = ci.console_view_entry(
        category = 'linux|blink',
        short_name = 'TD',
    ),
)

ci.fyi_builder(
    name = 'linux-blink-cors-rel',
    console_view_entry = ci.console_view_entry(
        category = 'linux',
    ),
)

ci.fyi_builder(
    name = 'linux-blink-heap-concurrent-marking-tsan-rel',
    console_view_entry = ci.console_view_entry(
        category = 'linux|blink',
        short_name = 'CM',
    ),
)

ci.fyi_builder(
    name = 'linux-blink-heap-verification',
    console_view_entry = ci.console_view_entry(
        category = 'linux|blink',
        short_name = 'VF',
    ),
)

ci.fyi_builder(
    name = 'linux-chromium-tests-staging-builder',
    console_view_entry = ci.console_view_entry(
        category = 'recipe|staging|linux',
        short_name = 'bld',
    ),
)

ci.fyi_builder(
    name = 'linux-chromium-tests-staging-tests',
    console_view_entry = ci.console_view_entry(
        category = 'recipe|staging|linux',
        short_name = 'tst',
    ),
    triggered_by = ['linux-chromium-tests-staging-builder'],
)

ci.fyi_builder(
    name = 'linux-fieldtrial-rel',
    console_view_entry = ci.console_view_entry(
        category = 'linux',
    ),
)

ci.fyi_builder(
    name = 'linux-perfetto-rel',
    console_view_entry = ci.console_view_entry(
        category = 'linux',
    ),
)

ci.fyi_builder(
    name = 'linux-wpt-fyi-rel',
    console_view_entry = ci.console_view_entry(
        category = 'linux',
    ),
    experimental = True,
    goma_backend = None,
)

ci.fyi_builder(
    name = 'win-pixel-builder-rel',
    console_view_entry = ci.console_view_entry(
        category = 'win10',
    ),
    os = os.WINDOWS_10,
)

ci.fyi_builder(
    name = 'win-pixel-tester-rel',
    console_view_entry = ci.console_view_entry(
        category = 'win10',
    ),
    os = None,
    triggered_by = ['win-pixel-builder-rel'],
)

ci.fyi_builder(
    name = 'linux-upload-perfetto',
    console_view_entry = ci.console_view_entry(
        category = 'perfetto',
        short_name = 'lnx',
    ),
    os = os.LINUX_DEFAULT,
)

ci.fyi_builder(
    name = 'mac-upload-perfetto',
    builderless = True,
    console_view_entry = ci.console_view_entry(
        category = 'perfetto',
        short_name = 'mac',
    ),
    os = os.MAC_DEFAULT,
    schedule = 'with 3h interval',
    triggered_by = [],
)

ci.fyi_builder(
    name = 'win-upload-perfetto',
    builderless = True,
    console_view_entry = ci.console_view_entry(
        category = 'perfetto',
        short_name = 'win',
    ),
    os = os.WINDOWS_DEFAULT,
    schedule = 'with 3h interval',
    triggered_by = [],
)

ci.fyi_celab_builder(
    name = 'win-celab-builder-rel',
    console_view_entry = ci.console_view_entry(
        category = 'celab',
    ),
    schedule = '0 0,6,12,18 * * *',
    triggered_by = [],
)

ci.fyi_celab_builder(
    name = 'win-celab-tester-rel',
    console_view_entry = ci.console_view_entry(
        category = 'celab',
    ),
    triggered_by = ['win-celab-builder-rel'],
)


ci.fyi_coverage_builder(
    name = 'android-code-coverage',
    console_view_entry = ci.console_view_entry(
        category = 'code_coverage',
        short_name = 'and',
    ),
    use_java_coverage = True,
    schedule = 'triggered',
    triggered_by = [],
)

ci.fyi_coverage_builder(
    name = 'android-code-coverage-native',
    console_view_entry = ci.console_view_entry(
        category = 'code_coverage',
        short_name = 'ann',
    ),
    use_clang_coverage = True,
)

ci.fyi_coverage_builder(
    name = 'ios-simulator-code-coverage',
    caches = [xcode_cache.x11c29],
    console_view_entry = ci.console_view_entry(
        category = 'code_coverage',
        short_name = 'ios',
    ),
    cores = None,
    os = os.MAC_ANY,
    use_clang_coverage = True,
    properties = {
        'coverage_exclude_sources': 'ios_test_files_and_test_utils',
        'coverage_test_types': ['overall', 'unit'],
        'xcode_build_version': '11c29',
    },
)

ci.fyi_coverage_builder(
    name = 'linux-chromeos-code-coverage',
    console_view_entry = ci.console_view_entry(
        category = 'code_coverage',
        short_name = 'lcr',
    ),
    use_clang_coverage = True,
    schedule = 'triggered',
    triggered_by = [],
)

ci.fyi_coverage_builder(
    name = 'linux-code-coverage',
    console_view_entry = ci.console_view_entry(
        category = 'code_coverage',
        short_name = 'lnx',
    ),
    use_clang_coverage = True,
    triggered_by = [],
)

ci.fyi_coverage_builder(
    name = 'mac-code-coverage',
    builderless = True,
    console_view_entry = ci.console_view_entry(
        category = 'code_coverage',
        short_name = 'mac',
    ),
    cores = 24,
    os = os.MAC_ANY,
    use_clang_coverage = True,
)

ci.fyi_coverage_builder(
    name = 'win10-code-coverage',
    builderless = True,
    console_view_entry = ci.console_view_entry(
        category = 'code_coverage',
        short_name = 'win',
    ),
    os = os.WINDOWS_DEFAULT,
    use_clang_coverage = True,
)


ci.fyi_ios_builder(
    name = 'ios-simulator-cr-recipe',
    console_view_entry = ci.console_view_entry(
        category = 'iOS',
        short_name = 'chr',
    ),
    executable = 'recipe:chromium',
    properties = {
        'xcode_build_version': '11a1027',
    },
)

ci.fyi_ios_builder(
    name = 'ios-webkit-tot',
    caches = [xcode_cache.x11n605cwk],
    console_view_entry = ci.console_view_entry(
        category = 'iOS',
        short_name = 'wk',
    ),
    executable = 'recipe:chromium',
    properties = {
        'xcode_build_version': '11n605cwk'
    },
    schedule = '0 1-23/6 * * *',
    triggered_by = [],
)

ci.fyi_ios_builder(
    name = 'ios13-beta-simulator',
    console_view_entry = ci.console_view_entry(
        category = 'iOS|iOS13',
        short_name = 'ios13',
    ),
    executable = 'recipe:chromium',
    properties = {
        'xcode_build_version': '11c29',
    },
)

ci.fyi_ios_builder(
    name = 'ios13-sdk-device',
    console_view_entry = ci.console_view_entry(
        category = 'iOS|iOS13',
        short_name = 'dev',
    ),
    executable = 'recipe:chromium',
    properties = {
        'xcode_build_version': '11c29',
    },
)

ci.fyi_ios_builder(
    name = 'ios13-sdk-simulator',
    console_view_entry = ci.console_view_entry(
        category = 'iOS|iOS13',
        short_name = 'sim',
    ),
    caches = [xcode_cache.x11e146],
    executable = 'recipe:chromium',
    os = os.MAC_10_15,
    properties = {
        'xcode_build_version': '11e146'
    }
)


ci.fyi_mac_builder(
    name = 'Mac Builder Next',
    console_view_entry = ci.console_view_entry(
        category = 'mac',
        short_name = 'bld',
    ),
    cores = None,
    os = None,
)

ci.thin_tester(
    name = 'Mac10.15 Tests',
    mastername = 'chromium.fyi',
    console_view_entry = ci.console_view_entry(
        category = 'mac',
        short_name = '15',
    ),
    triggered_by = ['Mac Builder Next'],
)

ci.fyi_mac_builder(
    name = 'Mac deterministic',
    console_view_entry = ci.console_view_entry(
        category = 'deterministic|mac',
        short_name = 'rel',
    ),
    cores = None,
    executable = 'recipe:swarming/deterministic_build',
    execution_timeout = 6 * time.hour,
)

ci.fyi_mac_builder(
    name = 'Mac deterministic (dbg)',
    console_view_entry = ci.console_view_entry(
        category = 'deterministic|mac',
        short_name = 'dbg',
    ),
    cores = None,
    executable = 'recipe:swarming/deterministic_build',
    execution_timeout = 6 * time.hour,
)

ci.fyi_mac_builder(
    name = 'mac-hermetic-upgrade-rel',
    console_view_entry = ci.console_view_entry(
        category = 'mac',
        short_name = 'herm',
    ),
    cores = 8,
)

ci.fyi_mac_builder(
    name = 'mac-mojo-rel',
    console_view_entry = ci.console_view_entry(
        category = 'mojo',
        short_name = 'mac',
    ),
    os = os.MAC_ANY,
)


ci.fyi_windows_builder(
    name = 'Win 10 Fast Ring',
    console_view_entry = ci.console_view_entry(
        category = 'win10',
    ),
    os = os.WINDOWS_10,
)

ci.fyi_windows_builder(
    name = 'win32-arm64-rel',
    console_view_entry = ci.console_view_entry(
        category = 'win32|arm64',
    ),
    cpu = cpu.X86,
    goma_jobs = goma.jobs.J150,
)

ci.fyi_windows_builder(
    name = 'win-annotator-rel',
    builderless = True,
    console_view_entry = ci.console_view_entry(
        category = 'network|traffic|annotations',
        short_name = 'win',
    ),
    execution_timeout = 16 * time.hour,
)

ci.fyi_windows_builder(
    name = 'Mojo Windows',
    console_view_entry = ci.console_view_entry(
        category = 'mojo',
        short_name = 'win',
    ),
)


ci.gpu_builder(
    name = 'GPU Linux Builder (dbg)',
    console_view_entry = ci.console_view_entry(
        category = 'Linux',
    ),
)

ci.gpu_builder(
    name = 'GPU Mac Builder (dbg)',
    console_view_entry = ci.console_view_entry(
        category = 'Mac',
    ),
    cores = None,
    os = os.MAC_ANY,
)

ci.gpu_builder(
    name = 'GPU Win x64 Builder (dbg)',
    builderless = True,
    console_view_entry = ci.console_view_entry(
        category = 'Windows',
    ),
    os = os.WINDOWS_ANY,
)


ci.gpu_thin_tester(
    name = 'Linux Debug (NVIDIA)',
    console_view_entry = ci.console_view_entry(
        category = 'Linux',
    ),
    triggered_by = ['GPU Linux Builder (dbg)'],
)

ci.gpu_thin_tester(
    name = 'Mac Debug (Intel)',
    console_view_entry = ci.console_view_entry(
        category = 'Mac',
    ),
    triggered_by = ['GPU Mac Builder (dbg)'],
)

ci.gpu_thin_tester(
    name = 'Mac Retina Debug (AMD)',
    console_view_entry = ci.console_view_entry(
        category = 'Mac',
    ),
    triggered_by = ['GPU Mac Builder (dbg)'],
)

ci.gpu_thin_tester(
    name = 'Win10 x64 Debug (NVIDIA)',
    console_view_entry = ci.console_view_entry(
        category = 'Windows',
    ),
    triggered_by = ['GPU Win x64 Builder (dbg)'],
)


ci.gpu_fyi_linux_builder(
    name = 'Android FYI 32 Vk Release (Pixel 2)',
    console_view_entry = ci.console_view_entry(
        category = 'Android|vk|Q32',
        short_name = 'P2',
    ),
)

ci.gpu_fyi_linux_builder(
    name = 'Android FYI 32 dEQP Vk Release (Pixel 2)',
    console_view_entry = ci.console_view_entry(
        category = 'Android|dqp|vk|Q32',
        short_name = 'P2',
    ),
)

ci.gpu_fyi_linux_builder(
    name = 'Android FYI 64 Perf (Pixel 2)',
    console_view_entry = ci.console_view_entry(
        category = 'Android|Perf|Q64',
        short_name = 'P2',
    ),
    cores = 2,
    triggered_by = ['GPU FYI Perf Android 64 Builder'],
)

ci.gpu_fyi_linux_builder(
    name = 'Android FYI 64 Vk Release (Pixel 2)',
    console_view_entry = ci.console_view_entry(
        category = 'Android|vk|Q64',
        short_name = 'P2',
    ),
)

ci.gpu_fyi_linux_builder(
    name = 'Android FYI 64 dEQP Vk Release (Pixel 2)',
    console_view_entry = ci.console_view_entry(
        category = 'Android|dqp|vk|Q64',
        short_name = 'P2',
    ),
)

ci.gpu_fyi_linux_builder(
    name = 'Android FYI Release (NVIDIA Shield TV)',
    console_view_entry = ci.console_view_entry(
        category = 'Android|N64|NVDA',
        short_name = 'STV',
    ),
)

ci.gpu_fyi_linux_builder(
    name = 'Android FYI Release (Nexus 5)',
    console_view_entry = ci.console_view_entry(
        category = 'Android|L32',
        short_name = 'N5',
    ),
)

ci.gpu_fyi_linux_builder(
    name = 'Android FYI Release (Nexus 5X)',
    console_view_entry = ci.console_view_entry(
        category = 'Android|M64|QCOM',
        short_name = 'N5X',
    ),
)

ci.gpu_fyi_linux_builder(
    name = 'Android FYI Release (Nexus 6)',
    console_view_entry = ci.console_view_entry(
        category = 'Android|L32',
        short_name = 'N6',
    ),
)

ci.gpu_fyi_linux_builder(
    name = 'Android FYI Release (Nexus 6P)',
    console_view_entry = ci.console_view_entry(
        category = 'Android|M64|QCOM',
        short_name = 'N6P',
    ),
)

ci.gpu_fyi_linux_builder(
    name = 'Android FYI Release (Nexus 9)',
    console_view_entry = ci.console_view_entry(
        category = 'Android|M64|NVDA',
        short_name = 'N9',
    ),
)

ci.gpu_fyi_linux_builder(
    name = 'Android FYI Release (Pixel 2)',
    console_view_entry = ci.console_view_entry(
        category = 'Android|P32|QCOM',
        short_name = 'P2',
    ),
)

ci.gpu_fyi_linux_builder(
    name = 'Android FYI SkiaRenderer GL (Nexus 5X)',
    console_view_entry = ci.console_view_entry(
        category = 'Android|skgl|M64',
        short_name = 'N5X',
    ),
)

ci.gpu_fyi_linux_builder(
    name = 'Android FYI SkiaRenderer Vulkan (Pixel 2)',
    console_view_entry = ci.console_view_entry(
        category = 'Android|skv|P32',
        short_name = 'P2',
    ),
)

ci.gpu_fyi_linux_builder(
    name = 'Android FYI dEQP Release (Nexus 5X)',
    console_view_entry = ci.console_view_entry(
        category = 'Android|dqp|M64',
        short_name = 'N5X',
    ),
)

ci.gpu_fyi_linux_builder(
    name = 'GPU FYI Linux Builder',
    console_view_entry = ci.console_view_entry(
        category = 'Linux|Builder',
        short_name = 'rel',
    ),
)

ci.gpu_fyi_linux_builder(
    name = 'GPU FYI Linux Builder (dbg)',
    console_view_entry = ci.console_view_entry(
        category = 'Linux|Builder',
        short_name = 'dbg',
    ),
)

ci.gpu_fyi_linux_builder(
    name = 'GPU FYI Linux Ozone Builder',
    console_view_entry = ci.console_view_entry(
        category = 'Linux|Builder',
        short_name = 'ozn',
    ),
)

ci.gpu_fyi_linux_builder(
    name = 'GPU FYI Linux dEQP Builder',
    console_view_entry = ci.console_view_entry(
        category = 'Linux|Builder',
        short_name = 'dqp',
    ),
)

ci.gpu_fyi_linux_builder(
    name = 'GPU FYI Perf Android 64 Builder',
    console_view_entry = ci.console_view_entry(
        category = 'Android|Perf|Builder',
        short_name = '64',
    ),
)

ci.gpu_fyi_linux_builder(
    name = 'Linux FYI GPU TSAN Release',
    console_view_entry = ci.console_view_entry(
        category = 'Linux',
        short_name = 'tsn',
    ),
)

# Builder + tester.
ci.gpu_fyi_linux_builder(
    name = 'Linux FYI SkiaRenderer Dawn Release (Intel HD 630)',
    console_view_entry = ci.console_view_entry(
        category = 'Linux|Intel',
        short_name = 'skd',
    ),
)


ci.gpu_fyi_mac_builder(
    name = 'Mac FYI GPU ASAN Release',
    console_view_entry = ci.console_view_entry(
        category = 'Mac',
        short_name = 'asn',
    ),
)

ci.gpu_fyi_mac_builder(
    name = 'GPU FYI Mac Builder',
    console_view_entry = ci.console_view_entry(
        category = 'Mac|Builder',
        short_name = 'rel',
    ),
)

ci.gpu_fyi_mac_builder(
    name = 'GPU FYI Mac Builder (dbg)',
    console_view_entry = ci.console_view_entry(
        category = 'Mac|Builder',
        short_name = 'dbg',
    ),
)

ci.gpu_fyi_mac_builder(
    name = 'GPU FYI Mac dEQP Builder',
    console_view_entry = ci.console_view_entry(
        category = 'Mac|Builder',
        short_name = 'dqp',
    ),
)


ci.gpu_fyi_thin_tester(
    name = 'Linux FYI Debug (NVIDIA)',
    console_view_entry = ci.console_view_entry(
        category = 'Linux|Nvidia',
        short_name = 'dbg',
    ),
    triggered_by = ['GPU FYI Linux Builder (dbg)'],
)

ci.gpu_fyi_thin_tester(
    name = 'Linux FYI Experimental Release (Intel HD 630)',
    console_view_entry = ci.console_view_entry(
        category = 'Linux|Intel',
        short_name = 'exp',
    ),
    triggered_by = ['GPU FYI Linux Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Linux FYI Experimental Release (NVIDIA)',
    console_view_entry = ci.console_view_entry(
        category = 'Linux|Nvidia',
        short_name = 'exp',
    ),
    triggered_by = ['GPU FYI Linux Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Linux FYI Ozone (Intel)',
    console_view_entry = ci.console_view_entry(
        category = 'Linux|Intel',
        short_name = 'ozn',
    ),
    triggered_by = ['GPU FYI Linux Ozone Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Linux FYI Release (NVIDIA)',
    console_view_entry = ci.console_view_entry(
        category = 'Linux|Nvidia',
        short_name = 'rel',
    ),
    triggered_by = ['GPU FYI Linux Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Linux FYI Release (AMD R7 240)',
    console_view_entry = ci.console_view_entry(
        category = 'Linux|AMD',
        short_name = 'rel',
    ),
    triggered_by = ['GPU FYI Linux Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Linux FYI Release (Intel HD 630)',
    console_view_entry = ci.console_view_entry(
        category = 'Linux|Intel',
        short_name = 'rel',
    ),
    triggered_by = ['GPU FYI Linux Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Linux FYI Release (Intel UHD 630)',
    console_view_entry = ci.console_view_entry(
        category = 'Linux|Intel',
        short_name = 'uhd',
    ),
    # TODO(https://crbug.com/986939): Remove this increased timeout once more
    # devices are added.
    execution_timeout = 18 * time.hour,
    triggered_by = ['GPU FYI Linux Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Linux FYI SkiaRenderer Vulkan (Intel HD 630)',
    console_view_entry = ci.console_view_entry(
        category = 'Linux|Intel',
        short_name = 'skv',
    ),
    triggered_by = ['GPU FYI Linux Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Linux FYI SkiaRenderer Vulkan (NVIDIA)',
    console_view_entry = ci.console_view_entry(
        category = 'Linux|Nvidia',
        short_name = 'skv',
    ),
    triggered_by = ['GPU FYI Linux Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Linux FYI dEQP Release (Intel HD 630)',
    console_view_entry = ci.console_view_entry(
        category = 'Linux|Intel',
        short_name = 'dqp',
    ),
    triggered_by = ['GPU FYI Linux dEQP Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Linux FYI dEQP Release (NVIDIA)',
    console_view_entry = ci.console_view_entry(
        category = 'Linux|Nvidia',
        short_name = 'dqp',
    ),
    triggered_by = ['GPU FYI Linux dEQP Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Mac FYI Debug (Intel)',
    console_view_entry = ci.console_view_entry(
        category = 'Mac|Intel',
        short_name = 'dbg',
    ),
    triggered_by = ['GPU FYI Mac Builder (dbg)'],
)

ci.gpu_fyi_thin_tester(
    name = 'Mac FYI Experimental Release (Intel)',
    console_view_entry = ci.console_view_entry(
        category = 'Mac|Intel',
        short_name = 'exp',
    ),
    triggered_by = ['GPU FYI Mac Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Mac FYI Experimental Retina Release (AMD)',
    console_view_entry = ci.console_view_entry(
        category = 'Mac|AMD|Retina',
        short_name = 'exp',
    ),
    triggered_by = ['GPU FYI Mac Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Mac FYI Experimental Retina Release (NVIDIA)',
    console_view_entry = ci.console_view_entry(
        category = 'Mac|Nvidia',
        short_name = 'exp',
    ),
    # This bot has one machine backing its tests at the moment.
    # If it gets more, this can be removed.
    # See crbug.com/853307 for more context.
    execution_timeout = 12 * time.hour,
    triggered_by = ['GPU FYI Mac Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Mac FYI Release (Intel)',
    console_view_entry = ci.console_view_entry(
        category = 'Mac|Intel',
        short_name = 'rel',
    ),
    triggered_by = ['GPU FYI Mac Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Mac FYI Retina Debug (AMD)',
    console_view_entry = ci.console_view_entry(
        category = 'Mac|AMD|Retina',
        short_name = 'dbg',
    ),
    triggered_by = ['GPU FYI Mac Builder (dbg)'],
)

ci.gpu_fyi_thin_tester(
    name = 'Mac FYI Retina Debug (NVIDIA)',
    console_view_entry = ci.console_view_entry(
        category = 'Mac|Nvidia',
        short_name = 'dbg',
    ),
    triggered_by = ['GPU FYI Mac Builder (dbg)'],
)

ci.gpu_fyi_thin_tester(
    name = 'Mac FYI Retina Release (AMD)',
    console_view_entry = ci.console_view_entry(
        category = 'Mac|AMD|Retina',
        short_name = 'rel',
    ),
    triggered_by = ['GPU FYI Mac Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Mac FYI Retina Release (NVIDIA)',
    console_view_entry = ci.console_view_entry(
        category = 'Mac|Nvidia',
        short_name = 'rel',
    ),
    triggered_by = ['GPU FYI Mac Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Mac FYI dEQP Release AMD',
    console_view_entry = ci.console_view_entry(
        category = 'Mac|AMD',
        short_name = 'dqp',
    ),
    triggered_by = ['GPU FYI Mac dEQP Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Mac FYI dEQP Release Intel',
    console_view_entry = ci.console_view_entry(
        category = 'Mac|Intel',
        short_name = 'dqp',
    ),
    triggered_by = ['GPU FYI Mac dEQP Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Mac Pro FYI Release (AMD)',
    console_view_entry = ci.console_view_entry(
        category = 'Mac|AMD|Pro',
        short_name = 'rel',
    ),
    triggered_by = ['GPU FYI Mac Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Win10 FYI x64 Debug (NVIDIA)',
    console_view_entry = ci.console_view_entry(
        category = 'Windows|10|x64|Nvidia',
        short_name = 'dbg',
    ),
    triggered_by = ['GPU FYI Win x64 Builder (dbg)'],
)

ci.gpu_fyi_thin_tester(
    name = 'Win10 FYI x64 DX12 Vulkan Debug (NVIDIA)',
    console_view_entry = ci.console_view_entry(
        category = 'Windows|10|x64|Nvidia|dx12vk',
        short_name = 'dbg',
    ),
    triggered_by = ['GPU FYI Win x64 DX12 Vulkan Builder (dbg)'],
)

ci.gpu_fyi_thin_tester(
    name = 'Win10 FYI x64 DX12 Vulkan Release (NVIDIA)',
    console_view_entry = ci.console_view_entry(
        category = 'Windows|10|x64|Nvidia|dx12vk',
        short_name = 'rel',
    ),
    triggered_by = ['GPU FYI Win x64 DX12 Vulkan Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Win10 FYI x64 Exp Release (Intel HD 630)',
    console_view_entry = ci.console_view_entry(
        category = 'Windows|10|x64|Intel',
        short_name = 'exp',
    ),
    triggered_by = ['GPU FYI Win x64 Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Win10 FYI x64 Exp Release (NVIDIA)',
    console_view_entry = ci.console_view_entry(
        category = 'Windows|10|x64|Nvidia',
        short_name = 'exp',
    ),
    triggered_by = ['GPU FYI Win x64 Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Win10 FYI x64 Release (AMD RX 550)',
    console_view_entry = ci.console_view_entry(
        category = 'Windows|10|x64|AMD',
        short_name = 'rel',
    ),
    triggered_by = ['GPU FYI Win x64 Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Win10 FYI x64 Release (Intel HD 630)',
    console_view_entry = ci.console_view_entry(
        category = 'Windows|10|x64|Intel',
        short_name = 'rel',
    ),
    triggered_by = ['GPU FYI Win x64 Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Win10 FYI x64 Release (Intel UHD 630)',
    console_view_entry = ci.console_view_entry(
        category = 'Windows|10|x64|Intel',
        short_name = 'uhd',
    ),
    # TODO(https://crbug.com/986939): Remove this increased timeout once
    # more devices are added.
    execution_timeout = 18 * time.hour,
    triggered_by = ['GPU FYI Win x64 Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Win10 FYI x64 Release (NVIDIA GeForce GTX 1660)',
    console_view_entry = ci.console_view_entry(
        category = 'Windows|10|x64|Nvidia',
        short_name = 'gtx',
    ),
    execution_timeout = 18 * time.hour,
    triggered_by = ['GPU FYI Win x64 Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Win10 FYI x64 Release (NVIDIA)',
    console_view_entry = ci.console_view_entry(
        category = 'Windows|10|x64|Nvidia',
        short_name = 'rel',
    ),
    triggered_by = ['GPU FYI Win x64 Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Win10 FYI x64 Release XR Perf (NVIDIA)',
    console_view_entry = ci.console_view_entry(
        category = 'Windows|10|x64|Nvidia',
        short_name = 'xr',
    ),
    triggered_by = ['GPU FYI XR Win x64 Builder'],
)

# Builder + tester.
ci.gpu_fyi_windows_builder(
    name = 'Win10 FYI x64 SkiaRenderer Dawn Release (NVIDIA)',
    console_view_entry = ci.console_view_entry(
        category = 'Windows|10|x64|Nvidia',
        short_name = 'skd',
    ),
)

ci.gpu_fyi_thin_tester(
    name = 'Win10 FYI x64 SkiaRenderer GL (NVIDIA)',
    console_view_entry = ci.console_view_entry(
        category = 'Windows|10|x64|Nvidia',
        short_name = 'skgl',
    ),
    triggered_by = ['GPU FYI Win x64 Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Win10 FYI x64 dEQP Release (Intel HD 630)',
    console_view_entry = ci.console_view_entry(
        category = 'Windows|10|x64|Intel',
        short_name = 'dqp',
    ),
    triggered_by = ['GPU FYI Win x64 dEQP Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Win10 FYI x64 dEQP Release (NVIDIA)',
    console_view_entry = ci.console_view_entry(
        category = 'Windows|10|x64|Nvidia',
        short_name = 'dqp',
    ),
    triggered_by = ['GPU FYI Win x64 dEQP Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Win10 FYI x86 Release (NVIDIA)',
    console_view_entry = ci.console_view_entry(
        category = 'Windows|10|x86|Nvidia',
        short_name = 'rel',
    ),
    triggered_by = ['GPU FYI Win Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Win7 FYI Debug (AMD)',
    console_view_entry = ci.console_view_entry(
        category = 'Windows|7|x86|AMD',
        short_name = 'dbg',
    ),
    triggered_by = ['GPU FYI Win Builder (dbg)'],
)

ci.gpu_fyi_thin_tester(
    name = 'Win7 FYI Release (AMD)',
    console_view_entry = ci.console_view_entry(
        category = 'Windows|7|x86|AMD',
        short_name = 'rel',
    ),
    triggered_by = ['GPU FYI Win Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Win7 FYI Release (NVIDIA)',
    console_view_entry = ci.console_view_entry(
        category = 'Windows|7|x86|Nvidia',
        short_name = 'rel',
    ),
    triggered_by = ['GPU FYI Win Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Win7 FYI dEQP Release (AMD)',
    console_view_entry = ci.console_view_entry(
        category = 'Windows|7|x86|AMD',
        short_name = 'dqp',
    ),
    triggered_by = ['GPU FYI Win dEQP Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Win7 FYI x64 Release (NVIDIA)',
    console_view_entry = ci.console_view_entry(
        category = 'Windows|7|x64|Nvidia',
        short_name = 'rel',
    ),
    triggered_by = ['GPU FYI Win x64 Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Win7 FYI x64 dEQP Release (NVIDIA)',
    console_view_entry = ci.console_view_entry(
        category = 'Windows|7|x64|Nvidia',
        short_name = 'dqp',
    ),
    triggered_by = ['GPU FYI Win x64 dEQP Builder'],
)


ci.gpu_fyi_windows_builder(
    name = 'GPU FYI Win Builder',
    console_view_entry = ci.console_view_entry(
        category = 'Windows|Builder|Release',
        short_name = 'x86',
    ),
)

ci.gpu_fyi_windows_builder(
    name = 'GPU FYI Win Builder (dbg)',
    console_view_entry = ci.console_view_entry(
        category = 'Windows|Builder|Debug',
        short_name = 'x86',
    ),
)

ci.gpu_fyi_windows_builder(
    name = 'GPU FYI Win dEQP Builder',
    console_view_entry = ci.console_view_entry(
        category = 'Windows|Builder|dEQP',
        short_name = 'x86',
    ),
)

ci.gpu_fyi_windows_builder(
    name = 'GPU FYI Win x64 Builder',
    console_view_entry = ci.console_view_entry(
        category = 'Windows|Builder|Release',
        short_name = 'x64',
    ),
)

ci.gpu_fyi_windows_builder(
    name = 'GPU FYI Win x64 Builder (dbg)',
    console_view_entry = ci.console_view_entry(
        category = 'Windows|Builder|Debug',
        short_name = 'x64',
    ),
)

ci.gpu_fyi_windows_builder(
    name = 'GPU FYI Win x64 dEQP Builder',
    console_view_entry = ci.console_view_entry(
        category = 'Windows|Builder|dEQP',
        short_name = 'x64',
    ),
)

ci.gpu_fyi_windows_builder(
    name = 'GPU FYI Win x64 DX12 Vulkan Builder',
    console_view_entry = ci.console_view_entry(
        category = 'Windows|Builder|dx12vk',
        short_name = 'rel',
    ),
)

ci.gpu_fyi_windows_builder(
    name = 'GPU FYI Win x64 DX12 Vulkan Builder (dbg)',
    console_view_entry = ci.console_view_entry(
        category = 'Windows|Builder|dx12vk',
        short_name = 'dbg',
    ),
)

ci.gpu_fyi_windows_builder(
    name = 'GPU FYI XR Win x64 Builder',
    console_view_entry = ci.console_view_entry(
        category = 'Windows|Builder|XR',
        short_name = 'x64',
    ),
)


ci.linux_builder(
    name = 'Cast Audio Linux',
    console_view_entry = ci.console_view_entry(
        category = 'cast',
        short_name = 'aud',
    ),
    ssd = True,
)

ci.linux_builder(
    name = 'Deterministic Fuchsia (dbg)',
    console_view_entry = ci.console_view_entry(
        category = 'fuchsia|x64',
        short_name = 'det',
    ),
    executable = 'recipe:swarming/deterministic_build',
    execution_timeout = 6 * time.hour,
    goma_jobs = None,
)

ci.linux_builder(
    name = 'Deterministic Linux',
    console_view_entry = ci.console_view_entry(
        category = 'release',
        short_name = 'det',
    ),
    executable = 'recipe:swarming/deterministic_build',
    execution_timeout = 6 * time.hour,
)

ci.linux_builder(
    name = 'Deterministic Linux (dbg)',
    console_view_entry = ci.console_view_entry(
        category = 'debug|builder',
        short_name = 'det',
    ),
    cores = 32,
    executable = 'recipe:swarming/deterministic_build',
    execution_timeout = 6 * time.hour,
)

ci.linux_builder(
    name = 'Leak Detection Linux',
    console_view = 'chromium.fyi',
    console_view_entry = ci.console_view_entry(
        category = 'linux',
        short_name = 'lk',
    ),
)

ci.linux_builder(
    name = 'Linux Builder (dbg)(32)',
    console_view_entry = ci.console_view_entry(
        category = 'debug|builder',
        short_name = '32',
    ),
)

ci.linux_builder(
    name = 'Network Service Linux',
    console_view_entry = ci.console_view_entry(
        category = 'release',
        short_name = 'nsl',
    ),
)

ci.linux_builder(
    name = 'fuchsia-x64-dbg',
    console_view_entry = ci.console_view_entry(
        category = 'fuchsia|x64',
        short_name = 'dbg',
    ),
    notifies = ['cr-fuchsia'],
)

ci.linux_builder(
    name = 'linux-gcc-rel',
    console_view_entry = ci.console_view_entry(
        category = 'release',
        short_name = 'gcc',
    ),
    goma_backend = None,
)

ci.linux_builder(
    name = 'linux-trusty-rel',
    console_view_entry = ci.console_view_entry(
        category = 'release',
        short_name = 'tru',
    ),
    os = os.LINUX_TRUSTY,
)

ci.linux_builder(
    name = 'linux_chromium_component_updater',
    executable = 'recipe:findit/chromium/update_components',
    schedule = '0 0,6,12,18 * * *',
    service_account = 'component-mapping-updater@chops-service-accounts.iam.gserviceaccount.com',
    triggered_by = [],
)


ci.mac_ios_builder(
    name = 'ios-device',
    console_view_entry = ci.console_view_entry(
        category = 'ios|default',
        short_name = 'dev',
    ),
    executable = 'recipe:chromium',
    # We don't have necessary capacity to run this configuration in CQ, but it
    # is part of the main waterfall
    main_console_view = 'main',
)

ci.mac_ios_builder(
    name = 'ios-simulator-noncq',
    console_view_entry = ci.console_view_entry(
        category = 'ios|default',
        short_name = 'non',
    ),
    # We don't have necessary capacity to run this configuration in CQ, but it
    # is part of the main waterfall
    main_console_view = 'main',
)


ci.memory_builder(
    name = 'Android CFI',
    # TODO(https://crbug.com/1008094) When this builder is not consistently
    # failing, remove the console_view value
    console_view = 'chromium.android.fyi',
    console_view_entry = ci.console_view_entry(
        category = 'memory',
        short_name = 'cfi',
    ),
    cores = 32,
    # TODO(https://crbug.com/919430) Remove the larger timeout once compile
    # times have been brought down to reasonable level
    execution_timeout = time.hour * 9 / 2,  # 4.5 (can't multiply float * duration)
)

ci.memory_builder(
    name = 'Linux CFI',
    console_view_entry = ci.console_view_entry(
        category = 'cfi',
        short_name = 'lnx',
    ),
    cores = 32,
    # TODO(thakis): Remove once https://crbug.com/927738 is resolved.
    execution_timeout = 4 * time.hour,
    goma_jobs = goma.jobs.MANY_JOBS_FOR_CI,
)

ci.memory_builder(
    name = 'Linux Chromium OS ASan LSan Builder',
    console_view_entry = ci.console_view_entry(
        category = 'cros|asan',
        short_name = 'bld',
    ),
    # TODO(crbug.com/1030593): Builds take more than 3 hours sometimes. Remove
    # once the builds are faster.
    execution_timeout = 6 * time.hour,
)

ci.memory_builder(
    name = 'Linux Chromium OS ASan LSan Tests (1)',
    console_view_entry = ci.console_view_entry(
        category = 'cros|asan',
        short_name = 'tst',
    ),
    triggered_by = ['Linux Chromium OS ASan LSan Builder'],
)

ci.memory_builder(
    name = 'Linux ChromiumOS MSan Builder',
    console_view_entry = ci.console_view_entry(
        category = 'cros|msan',
        short_name = 'bld',
    ),
)

ci.memory_builder(
    name = 'Linux ChromiumOS MSan Tests',
    console_view_entry = ci.console_view_entry(
        category = 'cros|msan',
        short_name = 'tst',
    ),
    triggered_by = ['Linux ChromiumOS MSan Builder'],
)

ci.memory_builder(
    name = 'Linux MSan Builder',
    console_view_entry = ci.console_view_entry(
        category = 'linux|msan',
        short_name = 'bld',
    ),
    goma_jobs = goma.jobs.MANY_JOBS_FOR_CI,
)

ci.memory_builder(
    name = 'Linux MSan Tests',
    console_view_entry = ci.console_view_entry(
        category = 'linux|msan',
        short_name = 'tst',
    ),
    triggered_by = ['Linux MSan Builder'],
)

ci.memory_builder(
    name = 'Mac ASan 64 Builder',
    builderless = False,
    console_view_entry = ci.console_view_entry(
        category = 'mac',
        short_name = 'bld',
    ),
    goma_debug = True,  # TODO(hinoka): Remove this after debugging.
    goma_jobs = None,
    cores = None,  # Swapping between 8 and 24
    os = os.MAC_DEFAULT,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 2,
    ),
)

ci.memory_builder(
    name = 'Mac ASan 64 Tests (1)',
    builderless = False,
    console_view_entry = ci.console_view_entry(
        category = 'mac',
        short_name = 'tst',
    ),
    os = os.MAC_DEFAULT,
    triggered_by = ['Mac ASan 64 Builder'],
)

ci.memory_builder(
    name = 'WebKit Linux ASAN',
    console_view_entry = ci.console_view_entry(
        category = 'linux|webkit',
        short_name = 'asn',
    ),
)

ci.memory_builder(
    name = 'WebKit Linux Leak',
    console_view_entry = ci.console_view_entry(
        category = 'linux|webkit',
        short_name = 'lk',
    ),
)

ci.memory_builder(
    name = 'WebKit Linux MSAN',
    console_view_entry = ci.console_view_entry(
        category = 'linux|webkit',
        short_name = 'msn',
    ),
)

ci.memory_builder(
    name = 'android-asan',
    console_view_entry = ci.console_view_entry(
        category = 'android',
        short_name = 'asn',
    ),
)

ci.memory_builder(
    name = 'win-asan',
    console_view_entry = ci.console_view_entry(
        category = 'win',
        short_name = 'asn',
    ),
    cores = 32,
    builderless = True,
    os = os.WINDOWS_DEFAULT,
)


ci.swangle_linux_builder(
    name = 'linux-swangle-chromium-x64',
    console_view_entry = ci.console_view_entry(
        category = 'Chromium|Linux',
        short_name = 'x64',
    ),
)

ci.swangle_linux_builder(
    name = 'linux-swangle-tot-angle-x64',
    console_view_entry = ci.console_view_entry(
        category = 'ToT ANGLE|Linux',
        short_name = 'x64',
    ),
)

ci.swangle_linux_builder(
    name = 'linux-swangle-tot-angle-x86',
    console_view_entry = ci.console_view_entry(
        category = 'ToT ANGLE|Linux',
        short_name = 'x86',
    ),
)

ci.swangle_linux_builder(
    name = 'linux-swangle-tot-swiftshader-x64',
    console_view_entry = ci.console_view_entry(
        category = 'ToT SwiftShader|Linux',
        short_name = 'x64',
    ),
)

ci.swangle_linux_builder(
    name = 'linux-swangle-tot-swiftshader-x86',
    console_view_entry = ci.console_view_entry(
        category = 'ToT SwiftShader|Linux',
        short_name = 'x86',
    ),
)

ci.swangle_linux_builder(
    name = 'linux-swangle-x64',
    console_view_entry = ci.console_view_entry(
        category = 'DEPS|Linux',
        short_name = 'x64',
    ),
)

ci.swangle_linux_builder(
    name = 'linux-swangle-x86',
    console_view_entry = ci.console_view_entry(
        category = 'DEPS|Linux',
        short_name = 'x86',
    ),
)


ci.swangle_mac_builder(
    name = 'mac-swangle-chromium-x64',
    console_view_entry = ci.console_view_entry(
        category = 'Chromium|Mac',
        short_name = 'x64',
    ),
)


ci.swangle_windows_builder(
    name = 'win-swangle-chromium-x86',
    console_view_entry = ci.console_view_entry(
        category = 'Chromium|Windows',
        short_name = 'x86',
    ),
)

ci.swangle_windows_builder(
    name = 'win-swangle-tot-angle-x64',
    console_view_entry = ci.console_view_entry(
        category = 'ToT ANGLE|Windows',
        short_name = 'x64',
    ),
)

ci.swangle_windows_builder(
    name = 'win-swangle-tot-angle-x86',
    console_view_entry = ci.console_view_entry(
        category = 'ToT ANGLE|Windows',
        short_name = 'x86',
    ),
)

ci.swangle_windows_builder(
    name = 'win-swangle-tot-swiftshader-x64',
    console_view_entry = ci.console_view_entry(
        category = 'ToT SwiftShader|Windows',
        short_name = 'x64',
    ),
)

ci.swangle_windows_builder(
    name = 'win-swangle-tot-swiftshader-x86',
    console_view_entry = ci.console_view_entry(
        category = 'ToT SwiftShader|Windows',
        short_name = 'x86',
    ),
)

ci.swangle_windows_builder(
    name = 'win-swangle-x64',
    console_view_entry = ci.console_view_entry(
        category = 'DEPS|Windows',
        short_name = 'x64',
    ),
)

ci.swangle_windows_builder(
    name = 'win-swangle-x86',
    console_view_entry = ci.console_view_entry(
        category = 'DEPS|Windows',
        short_name = 'x86',
    ),
)


ci.win_builder(
    name = 'WebKit Win10',
    console_view_entry = ci.console_view_entry(
        category = 'misc',
        short_name = 'wbk',
    ),
    triggered_by = ['Win Builder'],
)

ci.win_builder(
    name = 'Win Builder',
    console_view_entry = ci.console_view_entry(
        category = 'release|builder',
        short_name = '32',
    ),
    cores = 32,
    os = os.WINDOWS_ANY,
)

ci.win_builder(
    name = 'Win x64 Builder (dbg)',
    console_view_entry = ci.console_view_entry(
        category = 'debug|builder',
        short_name = '64',
    ),
    cores = 32,
    builderless = True,
    os = os.WINDOWS_ANY,
)

ci.win_builder(
    name = 'Win10 Tests x64 (dbg)',
    console_view_entry = ci.console_view_entry(
        category = 'debug|tester',
        short_name = '10',
    ),
    triggered_by = ['Win x64 Builder (dbg)'],
)

ci.win_builder(
    name = 'Win7 (32) Tests',
    console_view_entry = ci.console_view_entry(
        category = 'release|tester',
        short_name = '32',
    ),
    os = os.WINDOWS_7,
    triggered_by = ['Win Builder'],
)

ci.win_builder(
    name = 'Win7 Tests (1)',
    console_view_entry = ci.console_view_entry(
        category = 'release|tester',
        short_name = '32',
    ),
    os = os.WINDOWS_7,
    triggered_by = ['Win Builder'],
)

ci.win_builder(
    name = 'Windows deterministic',
    console_view_entry = ci.console_view_entry(
        category = 'misc',
        short_name = 'det',
    ),
    executable = 'recipe:swarming/deterministic_build',
    execution_timeout = 6 * time.hour,
)
