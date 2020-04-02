load('//lib/builders.star', 'cpu', 'goma', 'os', 'xcode_cache')
load('//lib/ci.star', 'ci')

# Defaults that apply to all branch versions of the bucket

ci.defaults.build_numbers.set(True)
ci.defaults.configure_kitchen.set(True)
ci.defaults.cores.set(8)
ci.defaults.cpu.set(cpu.X86_64)
ci.defaults.executable.set('recipe:chromium')
ci.defaults.execution_timeout.set(3 * time.hour)
ci.defaults.os.set(os.LINUX_DEFAULT)
ci.defaults.service_account.set(
    'chromium-ci-builder@chops-service-accounts.iam.gserviceaccount.com')
ci.defaults.swarming_tags.set(['vpython:native-python-wrapper'])


# Execute the versioned files to define all of the per-branch entities
# (bucket, builders, console, poller, etc.)
exec('//versioned/trunk/buckets/ci.star')
exec('//versioned/milestones/m80/buckets/ci.star')
exec('//versioned/milestones/m81/buckets/ci.star')


# *** After this point everything is trunk only ***
ci.defaults.bucket.set('ci')
ci.defaults.triggered_by.set(['master-gitiles-trigger'])


# Builders are sorted first lexicographically by the function used to define
# them, then lexicographically by their name


ci.builder(
    name = 'android-avd-packager',
    executable = 'recipe:android/avd_packager',
    properties = {
        'avd_configs': [
            'tools/android/avd/proto/generic_android23.textpb',
            'tools/android/avd/proto/generic_android28.textpb',
            'tools/android/avd/proto/generic_playstore_android28.textpb',
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
)

ci.android_builder(
    name = 'Android WebView L (dbg)',
    triggered_by = ['ci/Android arm Builder (dbg)'],
)

ci.android_builder(
    name = 'Deterministic Android',
    executable = 'recipe:swarming/deterministic_build',
    execution_timeout = 6 * time.hour,
)

ci.android_builder(
    name = 'Deterministic Android (dbg)',
    executable = 'recipe:swarming/deterministic_build',
    execution_timeout = 6 * time.hour,
)

ci.android_builder(
    name = 'KitKat Phone Tester (dbg)',
    triggered_by = ['ci/Android arm Builder (dbg)'],
)

ci.android_builder(
    name = 'KitKat Tablet Tester',
    # We have limited tablet capacity and thus limited ability to run
    # tests in parallel, hence the high timeout.
    execution_timeout = 20 * time.hour,
    triggered_by = ['ci/Android arm Builder (dbg)'],
)

ci.android_builder(
    name = 'Lollipop Phone Tester',
    # We have limited phone capacity and thus limited ability to run
    # tests in parallel, hence the high timeout.
    execution_timeout = 6 * time.hour,
    triggered_by = ['ci/Android arm Builder (dbg)'],
)

ci.android_builder(
    name = 'Lollipop Tablet Tester',
    # We have limited tablet capacity and thus limited ability to run
    # tests in parallel, hence the high timeout.
    execution_timeout = 20 * time.hour,
    triggered_by = ['ci/Android arm Builder (dbg)'],
)

ci.android_builder(
    name = 'Marshmallow Tablet Tester',
    # We have limited tablet capacity and thus limited ability to run
    # tests in parallel, hence the high timeout.
    execution_timeout = 12 * time.hour,
    triggered_by = ['ci/Android arm Builder (dbg)'],
)

ci.android_builder(
    name = 'android-arm64-proguard-rel',
    goma_jobs = goma.jobs.MANY_JOBS_FOR_CI,
    execution_timeout = 6 * time.hour,
)

ci.android_builder(
    name = 'android-cronet-arm64-dbg',
    notifies = ['cronet'],
)

ci.android_builder(
    name = 'android-cronet-arm64-rel',
    notifies = ['cronet'],
)

ci.android_builder(
    name = 'android-cronet-asan-arm-rel',
    notifies = ['cronet'],
)

# Runs on a specific machine with an attached phone
ci.android_builder(
    name = 'android-cronet-marshmallow-arm64-perf-rel',
    cores = None,
    cpu = None,
    executable = 'recipe:cronet',
    notifies = ['cronet'],
    os = os.ANDROID,
)

ci.android_builder(
    name = 'android-cronet-marshmallow-arm64-rel',
    notifies = ['cronet'],
    triggered_by = ['android-cronet-arm64-rel'],
)

ci.android_builder(
    name = 'android-cronet-x86-dbg',
    notifies = ['cronet'],
)

ci.android_builder(
    name = 'android-cronet-x86-rel',
    notifies = ['cronet'],
)

ci.android_builder(
    name = 'android-incremental-dbg',
)

ci.android_builder(
    name = 'android-lollipop-arm-rel',
)

ci.android_builder(
    name = 'android-pie-x86-rel',
)

ci.android_builder(
    name = 'android-10-arm64-rel',
)


ci.android_fyi_builder(
    name = 'android-bfcache-rel',
)

ci.android_fyi_builder(
    name = 'Android WebLayer P FYI (rel)',
)

ci.android_fyi_builder(
    name = 'Android WebView P Blink-CORS FYI (rel)',
)

ci.android_fyi_builder(
    name = 'Android WebView P FYI (rel)',
)

ci.android_fyi_builder(
    name = 'android-marshmallow-x86-fyi-rel',
    schedule = '0 7 * * *',
    triggered_by = [],
)

# TODO(hypan): remove this once there is no associated disabled tests
ci.android_fyi_builder(
    name = 'android-pie-x86-fyi-rel',
    goma_jobs=goma.jobs.J150,
    schedule = 'triggered',  # triggered manually via Scheduler UI
)


ci.chromium_builder(
    name = 'android-archive-dbg',
    # Bump to 32 if needed.
    cores = 8,
)

ci.chromium_builder(
    name = 'android-archive-rel',
    cores = 32,
)

ci.chromium_builder(
    name = 'linux-archive-dbg',
    # Bump to 32 if needed.
    cores = 8,
)

ci.chromium_builder(
    name = 'linux-archive-rel',
    cores = 32,
)

ci.chromium_builder(
    name = 'mac-archive-dbg',
    # Bump to 8 cores if needed.
    cores = 4,
    os = os.MAC_DEFAULT,
)

ci.chromium_builder(
    name = 'mac-archive-rel',
    os = os.MAC_DEFAULT,
)

ci.chromium_builder(
    name = 'win-archive-dbg',
    cores = 32,
    os = os.WINDOWS_DEFAULT,
)

ci.chromium_builder(
    name = 'win-archive-rel',
    cores = 32,
    os = os.WINDOWS_DEFAULT,
)

ci.chromium_builder(
    name = 'win32-archive-dbg',
    cores = 32,
    os = os.WINDOWS_DEFAULT,
)

ci.chromium_builder(
    name = 'win32-archive-rel',
    cores = 32,
    os = os.WINDOWS_DEFAULT,
)


ci.chromiumos_builder(
    name = 'Linux ChromiumOS Full',
)

ci.chromiumos_builder(
    name = 'chromeos-amd64-generic-asan-rel',
)

ci.chromiumos_builder(
    name = 'chromeos-amd64-generic-cfi-thin-lto-rel',
)

ci.chromiumos_builder(
    name = 'chromeos-arm-generic-dbg',
)


ci.clang_builder(
    name = 'CFI Linux CF',
    goma_backend = goma.backend.RBE_PROD,
)

ci.clang_builder(
    name = 'CFI Linux ToT',
)

ci.clang_builder(
    name = 'CrWinAsan',
    os = os.WINDOWS_ANY,
)

ci.clang_builder(
    name = 'CrWinAsan(dll)',
    os = os.WINDOWS_ANY,
)

ci.clang_builder(
    name = 'ToTAndroid',
)

ci.clang_builder(
    name = 'ToTAndroid (dbg)',
)

ci.clang_builder(
    name = 'ToTAndroid x64',
)

ci.clang_builder(
    name = 'ToTAndroid64',
)

ci.clang_builder(
    name = 'ToTAndroidASan',
)

ci.clang_builder(
    name = 'ToTAndroidCFI',
)

ci.clang_builder(
    name = 'ToTAndroidOfficial',
)

ci.clang_builder(
    name = 'ToTLinux',
)

ci.clang_builder(
    name = 'ToTLinux (dbg)',
)

ci.clang_builder(
    name = 'ToTLinuxASan',
)

ci.clang_builder(
    name = 'ToTLinuxASanLibfuzzer',
)

ci.clang_builder(
    name = 'ToTLinuxCoverage',
    executable = 'recipe:chromium_clang_coverage_tot',
)

ci.clang_builder(
    name = 'ToTLinuxMSan',
)

ci.clang_builder(
    name = 'ToTLinuxTSan',
)

ci.clang_builder(
    name = 'ToTLinuxThinLTO',
)

ci.clang_builder(
    name = 'ToTLinuxUBSanVptr',
)

ci.clang_builder(
    name = 'ToTWin(dbg)',
    os = os.WINDOWS_ANY,
)

ci.clang_builder(
    name = 'ToTWin(dll)',
    os = os.WINDOWS_ANY,
)

ci.clang_builder(
    name = 'ToTWin64(dbg)',
    os = os.WINDOWS_ANY,
)

ci.clang_builder(
    name = 'ToTWin64(dll)',
    os = os.WINDOWS_ANY,
)

ci.clang_builder(
    name = 'ToTWinASanLibfuzzer',
    os = os.WINDOWS_ANY,
)

ci.clang_builder(
    name = 'ToTWinCFI',
    os = os.WINDOWS_ANY,
)

ci.clang_builder(
    name = 'ToTWinCFI64',
    os = os.WINDOWS_ANY,
)

ci.clang_builder(
    name = 'UBSanVptr Linux',
    goma_backend = goma.backend.RBE_PROD,
)

ci.clang_builder(
    name = 'linux-win_cross-rel',
)

ci.clang_builder(
    name = 'ToTiOS',
    caches = [xcode_cache.x11c29],
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
    cores = None,
    os = os.MAC_10_14,
    properties = {
        'xcode_build_version': '11c29'
    },
    ssd=True
)


ci.clang_mac_builder(
    name = 'ToTMac',
)

ci.clang_mac_builder(
    name = 'ToTMac (dbg)',
)

ci.clang_mac_builder(
    name = 'ToTMacASan',
)

ci.clang_mac_builder(
    name = 'ToTMacCoverage',
    executable = 'recipe:chromium_clang_coverage_tot',
)


ci.dawn_builder(
    name = 'Dawn Linux x64 Builder',
)

ci.dawn_builder(
    name = 'Dawn Linux x64 Release (Intel HD 630)',
    cores = 2,
    os = os.LINUX_DEFAULT,
    triggered_by = ['Dawn Linux x64 Builder'],
)

ci.dawn_builder(
    name = 'Dawn Linux x64 Release (NVIDIA)',
    cores = 2,
    os = os.LINUX_DEFAULT,
    triggered_by = ['Dawn Linux x64 Builder'],
)

ci.dawn_builder(
    name = 'Dawn Mac x64 Builder',
    builderless = False,
    cores = None,
    os = os.MAC_ANY,
)

ci.dawn_builder(
    name = 'Dawn Mac x64 Release (AMD)',
    cores = 2,
    os = os.LINUX_DEFAULT,
    triggered_by = ['Dawn Mac x64 Builder'],
)

ci.dawn_builder(
    name = 'Dawn Mac x64 Release (Intel)',
    cores = 2,
    os = os.LINUX_DEFAULT,
    triggered_by = ['Dawn Mac x64 Builder'],
)

ci.dawn_builder(
    name = 'Dawn Win10 x86 Builder',
    os = os.WINDOWS_ANY,
)

ci.dawn_builder(
    name = 'Dawn Win10 x64 Builder',
    os = os.WINDOWS_ANY,
)

# Note that the Win testers are all thin Linux VMs, triggering jobs on the
# physical Win hardware in the Swarming pool, which is why they run on linux
ci.dawn_builder(
    name = 'Dawn Win10 x86 Release (Intel HD 630)',
    cores = 2,
    os = os.LINUX_DEFAULT,
    triggered_by = ['Dawn Win10 x86 Builder'],
)

ci.dawn_builder(
    name = 'Dawn Win10 x64 Release (Intel HD 630)',
    cores = 2,
    os = os.LINUX_DEFAULT,
    triggered_by = ['Dawn Win10 x64 Builder'],
)

ci.dawn_builder(
    name = 'Dawn Win10 x86 Release (NVIDIA)',
    cores = 2,
    os = os.LINUX_DEFAULT,
    triggered_by = ['Dawn Win10 x86 Builder'],
)

ci.dawn_builder(
    name = 'Dawn Win10 x64 Release (NVIDIA)',
    cores = 2,
    os = os.LINUX_DEFAULT,
    triggered_by = ['Dawn Win10 x64 Builder'],
)


ci.fuzz_builder(
    name = 'ASAN Debug',
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.fuzz_builder(
    name = 'ASan Debug (32-bit x86 with V8-ARM)',
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.fuzz_builder(
    name = 'ASAN Release',
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 5,
    ),
)

ci.fuzz_builder(
    name = 'ASan Release (32-bit x86 with V8-ARM)',
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.fuzz_builder(
    name = 'ASAN Release Media',
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.fuzz_builder(
    name = 'Afl Upload Linux ASan',
    executable = 'recipe:chromium_afl',
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.fuzz_builder(
    name = 'ASan Release Media (32-bit x86 with V8-ARM)',
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.fuzz_builder(
    name = 'ChromiumOS ASAN Release',
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 6,
    ),
)

ci.fuzz_builder(
    name = 'MSAN Release (chained origins)',
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.fuzz_builder(
    name = 'MSAN Release (no origins)',
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.fuzz_builder(
    name = 'Mac ASAN Release',
    builderless = False,
    cores = 4,
    os = os.MAC_DEFAULT,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 2,
    ),
)

ci.fuzz_builder(
    name = 'Mac ASAN Release Media',
    builderless = False,
    cores = 4,
    os = os.MAC_DEFAULT,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 2,
    ),
)

ci.fuzz_builder(
    name = 'TSAN Debug',
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.fuzz_builder(
    name = 'TSAN Release',
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 3,
    ),
)

ci.fuzz_builder(
    name = 'UBSan Release',
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.fuzz_builder(
    name = 'UBSan vptr Release',
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.fuzz_builder(
    name = 'Win ASan Release',
    builderless = False,
    os = os.WINDOWS_DEFAULT,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 7,
    ),
)

ci.fuzz_builder(
    name = 'Win ASan Release Media',
    builderless = False,
    os = os.WINDOWS_DEFAULT,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 6,
    ),
)


ci.fuzz_libfuzzer_builder(
    name = 'Libfuzzer Upload Chrome OS ASan',
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 3,
    ),
)

ci.fuzz_libfuzzer_builder(
    name = 'Libfuzzer Upload Linux ASan',
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 5,
    ),
)

ci.fuzz_libfuzzer_builder(
    name = 'Libfuzzer Upload Linux ASan Debug',
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 5,
    ),
)

ci.fuzz_libfuzzer_builder(
    name = 'Libfuzzer Upload Linux MSan',
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 5,
    ),
)

ci.fuzz_libfuzzer_builder(
    name = 'Libfuzzer Upload Linux UBSan',
    # Do not use builderless for this (crbug.com/980080).
    builderless = False,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 5,
    ),
)

ci.fuzz_libfuzzer_builder(
    name = 'Libfuzzer Upload Linux V8-ARM64 ASan',
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 1,
    ),
)

ci.fuzz_libfuzzer_builder(
    name = 'Libfuzzer Upload Linux V8-ARM64 ASan Debug',
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 1,
    ),
)

ci.fuzz_libfuzzer_builder(
    name = 'Libfuzzer Upload Linux32 ASan',
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 3,
    ),
)

ci.fuzz_libfuzzer_builder(
    name = 'Libfuzzer Upload Linux32 ASan Debug',
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 3,
    ),
)

ci.fuzz_libfuzzer_builder(
    name = 'Libfuzzer Upload Linux32 V8-ARM ASan',
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 1,
    ),
)

ci.fuzz_libfuzzer_builder(
    name = 'Libfuzzer Upload Linux32 V8-ARM ASan Debug',
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 1,
    ),
)

ci.fuzz_libfuzzer_builder(
    name = 'Libfuzzer Upload Mac ASan',
    cores = 24,
    execution_timeout = 4 * time.hour,
    os = os.MAC_DEFAULT,
)

ci.fuzz_libfuzzer_builder(
    name = 'Libfuzzer Upload Windows ASan',
    os = os.WINDOWS_DEFAULT,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 3,
    ),
)


ci.fyi_builder(
    name = 'Closure Compilation Linux',
    executable = 'recipe:closure_compilation',
)

ci.fyi_builder(
    name = 'Linux Viz',
)

ci.fyi_builder(
    name = 'Linux remote_run Builder',
)

ci.fyi_builder(
    name = 'Linux remote_run Tester',
    triggered_by = ['Linux remote_run Builder'],
)

ci.fyi_builder(
    name = 'Mojo Android',
)

ci.fyi_builder(
    name = 'Mojo ChromiumOS',
)

ci.fyi_builder(
    name = 'Mojo Linux',
)

ci.fyi_builder(
    name = 'Site Isolation Android',
)

ci.fyi_builder(
    name = 'android-mojo-webview-rel',
)

ci.fyi_builder(
    name = 'chromeos-amd64-generic-rel-vm-tests',
)

ci.fyi_builder(
    name = 'fuchsia-fyi-arm64-rel',
    notifies = ['cr-fuchsia'],
)

ci.fyi_builder(
    name = 'fuchsia-fyi-x64-dbg',
    notifies = ['cr-fuchsia'],
)

ci.fyi_builder(
    name = 'fuchsia-fyi-x64-rel',
    notifies = ['cr-fuchsia'],
)

ci.fyi_builder(
    name = 'linux-annotator-rel',
)

ci.fyi_builder(
    name = 'linux-bfcache-rel',
)

ci.fyi_builder(
    name = 'linux-blink-animation-use-time-delta',
)

ci.fyi_builder(
    name = 'linux-blink-cors-rel',
)

ci.fyi_builder(
    name = 'linux-blink-heap-concurrent-marking-tsan-rel',
)

ci.fyi_builder(
    name = 'linux-blink-heap-verification',
)

ci.fyi_builder(
    name = 'linux-chromium-tests-staging-builder',
)

ci.fyi_builder(
    name = 'linux-chromium-tests-staging-tests',
    triggered_by = ['linux-chromium-tests-staging-builder'],
)

ci.fyi_builder(
    name = 'linux-fieldtrial-rel',
)

ci.fyi_builder(
    name = 'linux-wpt-fyi-rel',
    experimental = True,
    goma_backend = None,
)

ci.fyi_builder(
    name = 'win-pixel-builder-rel',
    os = os.WINDOWS_10,
)

ci.fyi_builder(
    name = 'win-pixel-tester-rel',
    os = None,
    triggered_by = ['win-pixel-builder-rel'],
)

ci.fyi_builder(
    name = 'linux-upload-perfetto',
    os = os.LINUX_DEFAULT,
)

ci.fyi_builder(
    builderless = True,
    name = 'mac-upload-perfetto',
    os = os.MAC_DEFAULT,
    schedule = 'with 3h interval',
    triggered_by = [],
)

ci.fyi_builder(
    builderless = True,
    name = 'win-upload-perfetto',
    os = os.WINDOWS_DEFAULT,
    schedule = 'with 3h interval',
    triggered_by = [],
)

ci.fyi_celab_builder(
    name = 'win-celab-builder-rel',
    schedule = '0 0,6,12,18 * * *',
    triggered_by = [],
)

ci.fyi_celab_builder(
    name = 'win-celab-tester-rel',
    triggered_by = ['win-celab-builder-rel'],
)


ci.fyi_coverage_builder(
    name = 'android-code-coverage',
    use_java_coverage = True,
    schedule = 'triggered',
    triggered_by = [],
)

ci.fyi_coverage_builder(
    name = 'android-code-coverage-native',
    use_clang_coverage = True,
)

ci.fyi_coverage_builder(
    name = 'ios-simulator-code-coverage',
    caches = [xcode_cache.x11c29],
    cores = None,
    os = os.MAC_ANY,
    use_clang_coverage = True,
    properties = {
        'xcode_build_version': '11c29',
        'coverage_test_types': ['overall', 'unit'],
    },
)

ci.fyi_coverage_builder(
    name = 'linux-chromeos-code-coverage',
    use_clang_coverage = True,
    schedule = 'triggered',
    triggered_by = [],
)

ci.fyi_coverage_builder(
    name = 'linux-code-coverage',
    use_clang_coverage = True,
    triggered_by = [],
)

ci.fyi_coverage_builder(
    name = 'mac-code-coverage',
    builderless = True,
    cores = 24,
    os = os.MAC_ANY,
    use_clang_coverage = True,
)

ci.fyi_coverage_builder(
    name = 'win10-code-coverage',
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    use_clang_coverage = True,
)


ci.fyi_ios_builder(
    name = 'ios-simulator-cr-recipe',
    executable = 'recipe:chromium',
    properties = {
        'xcode_build_version': '11a1027',
    },
)

ci.fyi_ios_builder(
    name = 'ios-simulator-cronet',
    executable = 'recipe:chromium',
    notifies = ['cronet'],
    properties = {
        'xcode_build_version': '11c29',
    },
)

ci.fyi_ios_builder(
    name = 'ios-webkit-tot',
    caches = [xcode_cache.x11c505wk],
    executable = 'recipe:chromium',
    properties = {
        'xcode_build_version': '11c505wk'
    },
    schedule = '0 1-23/6 * * *',
    triggered_by = [],
)

ci.fyi_ios_builder(
    name = 'ios13-beta-simulator',
    executable = 'recipe:chromium',
    properties = {
        'xcode_build_version': '11c29',
    },
)

ci.fyi_ios_builder(
    name = 'ios13-sdk-device',
    executable = 'recipe:chromium',
    properties = {
        'xcode_build_version': '11c29',
    },
)

ci.fyi_ios_builder(
    name = 'ios13-sdk-simulator',
    executable = 'recipe:chromium',
    properties = {
        'xcode_build_version': '11c29'
    }
)


ci.fyi_mac_builder(
    name = 'Mac Builder Next',
    cores = None,
    os = None,
)

ci.fyi_mac_builder(
    name = 'Mac10.15 Tests',
    cores = None,
    os = os.MAC_10_15,
    triggered_by = ['Mac Builder Next'],
)

ci.fyi_mac_builder(
    name = 'Mac deterministic',
    cores = None,
    executable = 'recipe:swarming/deterministic_build',
    execution_timeout = 6 * time.hour,
)

ci.fyi_mac_builder(
    name = 'Mac deterministic (dbg)',
    cores = None,
    executable = 'recipe:swarming/deterministic_build',
    execution_timeout = 6 * time.hour,
)

ci.fyi_mac_builder(
    name = 'mac-hermetic-upgrade-rel',
    cores = 8,
)

ci.fyi_mac_builder(
    name = 'mac-mojo-rel',
    os = os.MAC_ANY,
)


ci.fyi_windows_builder(
    name = 'Win 10 Fast Ring',
    os = os.WINDOWS_10,
)

ci.fyi_windows_builder(
    name = 'win32-arm64-rel',
    cpu = cpu.X86,
    goma_jobs = goma.jobs.J150,
)

ci.fyi_windows_builder(
    name = 'win-annotator-rel',
    builderless = True,
    execution_timeout = 16 * time.hour,
)

ci.fyi_windows_builder(
    name = 'Mojo Windows',
)


ci.gpu_builder(
    name = 'GPU Linux Builder (dbg)',
)

ci.gpu_builder(
    name = 'GPU Mac Builder (dbg)',
    cores = None,
    os = os.MAC_ANY,
)

ci.gpu_builder(
    name = 'GPU Win x64 Builder (dbg)',
    builderless = True,
    os = os.WINDOWS_ANY,
)


ci.gpu_thin_tester(
    name = 'Linux Debug (NVIDIA)',
    triggered_by = ['GPU Linux Builder (dbg)'],
)

ci.gpu_thin_tester(
    name = 'Mac Debug (Intel)',
    triggered_by = ['GPU Mac Builder (dbg)'],
)

ci.gpu_thin_tester(
    name = 'Mac Retina Debug (AMD)',
    triggered_by = ['GPU Mac Builder (dbg)'],
)

ci.gpu_thin_tester(
    name = 'Win10 x64 Debug (NVIDIA)',
    triggered_by = ['GPU Win x64 Builder (dbg)'],
)


ci.gpu_fyi_linux_builder(
    name = 'Android FYI 32 Vk Release (Pixel 2)',
)

ci.gpu_fyi_linux_builder(
    name = 'Android FYI 32 dEQP Vk Release (Pixel 2)',
)

ci.gpu_fyi_linux_builder(
    name = 'Android FYI 64 Perf (Pixel 2)',
    cores = 2,
    triggered_by = ['GPU FYI Perf Android 64 Builder'],
)

ci.gpu_fyi_linux_builder(
    name = 'Android FYI 64 Vk Release (Pixel 2)',
)

ci.gpu_fyi_linux_builder(
    name = 'Android FYI 64 dEQP Vk Release (Pixel 2)',
)

ci.gpu_fyi_linux_builder(
    name = 'Android FYI Release (NVIDIA Shield TV)',
)

ci.gpu_fyi_linux_builder(
    name = 'Android FYI Release (Nexus 5)',
)

ci.gpu_fyi_linux_builder(
    name = 'Android FYI Release (Nexus 5X)',
)

ci.gpu_fyi_linux_builder(
    name = 'Android FYI Release (Nexus 6)',
)

ci.gpu_fyi_linux_builder(
    name = 'Android FYI Release (Nexus 6P)',
)

ci.gpu_fyi_linux_builder(
    name = 'Android FYI Release (Nexus 9)',
)

ci.gpu_fyi_linux_builder(
    name = 'Android FYI Release (Pixel 2)',
)

ci.gpu_fyi_linux_builder(
    name = 'Android FYI SkiaRenderer GL (Nexus 5X)',
)

ci.gpu_fyi_linux_builder(
    name = 'Android FYI SkiaRenderer Vulkan (Pixel 2)',
)

ci.gpu_fyi_linux_builder(
    name = 'Android FYI dEQP Release (Nexus 5X)',
)

ci.gpu_fyi_linux_builder(
    name = 'GPU FYI Linux Builder',
)

ci.gpu_fyi_linux_builder(
    name = 'GPU FYI Linux Builder (dbg)',
)

ci.gpu_fyi_linux_builder(
    name = 'GPU FYI Linux Ozone Builder',
)

ci.gpu_fyi_linux_builder(
    name = 'GPU FYI Linux dEQP Builder',
)

ci.gpu_fyi_linux_builder(
    name = 'GPU FYI Perf Android 64 Builder',
)

ci.gpu_fyi_linux_builder(
    name = 'Linux FYI GPU TSAN Release',
)

# Builder + tester.
ci.gpu_fyi_linux_builder(
    name = 'Linux FYI SkiaRenderer Dawn Release (Intel HD 630)',
)


ci.gpu_fyi_mac_builder(
    name = 'Mac FYI GPU ASAN Release',
)

ci.gpu_fyi_mac_builder(
    name = 'GPU FYI Mac Builder',
)

ci.gpu_fyi_mac_builder(
    name = 'GPU FYI Mac Builder (dbg)',
)

ci.gpu_fyi_mac_builder(
    name = 'GPU FYI Mac dEQP Builder',
)


ci.gpu_fyi_thin_tester(
    name = 'Linux FYI Debug (NVIDIA)',
    triggered_by = ['GPU FYI Linux Builder (dbg)'],
)

ci.gpu_fyi_thin_tester(
    name = 'Linux FYI Experimental Release (Intel HD 630)',
    triggered_by = ['GPU FYI Linux Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Linux FYI Experimental Release (NVIDIA)',
    triggered_by = ['GPU FYI Linux Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Linux FYI Ozone (Intel)',
    triggered_by = ['GPU FYI Linux Ozone Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Linux FYI Release (NVIDIA)',
    triggered_by = ['GPU FYI Linux Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Linux FYI Release (AMD R7 240)',
    triggered_by = ['GPU FYI Linux Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Linux FYI Release (Intel HD 630)',
    triggered_by = ['GPU FYI Linux Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Linux FYI Release (Intel UHD 630)',
    # TODO(https://crbug.com/986939): Remove this increased timeout once more
    # devices are added.
    execution_timeout = 18 * time.hour,
    triggered_by = ['GPU FYI Linux Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Linux FYI SkiaRenderer Vulkan (Intel HD 630)',
    triggered_by = ['GPU FYI Linux Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Linux FYI SkiaRenderer Vulkan (NVIDIA)',
    triggered_by = ['GPU FYI Linux Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Linux FYI dEQP Release (Intel HD 630)',
    triggered_by = ['GPU FYI Linux dEQP Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Linux FYI dEQP Release (NVIDIA)',
    triggered_by = ['GPU FYI Linux dEQP Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Mac FYI Debug (Intel)',
    triggered_by = ['GPU FYI Mac Builder (dbg)'],
)

ci.gpu_fyi_thin_tester(
    name = 'Mac FYI Experimental Release (Intel)',
    triggered_by = ['GPU FYI Mac Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Mac FYI Experimental Retina Release (AMD)',
    triggered_by = ['GPU FYI Mac Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Mac FYI Experimental Retina Release (NVIDIA)',
    # This bot has one machine backing its tests at the moment.
    # If it gets more, this can be removed.
    # See crbug.com/853307 for more context.
    execution_timeout = 12 * time.hour,
    triggered_by = ['GPU FYI Mac Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Mac FYI Release (Intel)',
    triggered_by = ['GPU FYI Mac Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Mac FYI Retina Debug (AMD)',
    triggered_by = ['GPU FYI Mac Builder (dbg)'],
)

ci.gpu_fyi_thin_tester(
    name = 'Mac FYI Retina Debug (NVIDIA)',
    triggered_by = ['GPU FYI Mac Builder (dbg)'],
)

ci.gpu_fyi_thin_tester(
    name = 'Mac FYI Retina Release (AMD)',
    triggered_by = ['GPU FYI Mac Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Mac FYI Retina Release (NVIDIA)',
    triggered_by = ['GPU FYI Mac Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Mac FYI dEQP Release AMD',
    triggered_by = ['GPU FYI Mac dEQP Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Mac FYI dEQP Release Intel',
    triggered_by = ['GPU FYI Mac dEQP Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Mac Pro FYI Release (AMD)',
    triggered_by = ['GPU FYI Mac Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Win10 FYI x64 Debug (NVIDIA)',
    triggered_by = ['GPU FYI Win x64 Builder (dbg)'],
)

ci.gpu_fyi_thin_tester(
    name = 'Win10 FYI x64 DX12 Vulkan Debug (NVIDIA)',
    triggered_by = ['GPU FYI Win x64 DX12 Vulkan Builder (dbg)'],
)

ci.gpu_fyi_thin_tester(
    name = 'Win10 FYI x64 DX12 Vulkan Release (NVIDIA)',
    triggered_by = ['GPU FYI Win x64 DX12 Vulkan Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Win10 FYI x64 Exp Release (Intel HD 630)',
    triggered_by = ['GPU FYI Win x64 Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Win10 FYI x64 Exp Release (NVIDIA)',
    triggered_by = ['GPU FYI Win x64 Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Win10 FYI x64 Release (AMD RX 550)',
    triggered_by = ['GPU FYI Win x64 Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Win10 FYI x64 Release (Intel HD 630)',
    triggered_by = ['GPU FYI Win x64 Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Win10 FYI x64 Release (Intel UHD 630)',
    # TODO(https://crbug.com/986939): Remove this increased timeout once
    # more devices are added.
    execution_timeout = 18 * time.hour,
    triggered_by = ['GPU FYI Win x64 Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Win10 FYI x64 Release (NVIDIA GeForce GTX 1660)',
    execution_timeout = 18 * time.hour,
    triggered_by = ['GPU FYI Win x64 Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Win10 FYI x64 Release (NVIDIA)',
    triggered_by = ['GPU FYI Win x64 Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Win10 FYI x64 Release XR Perf (NVIDIA)',
    triggered_by = ['GPU FYI XR Win x64 Builder'],
)

# Builder + tester.
ci.gpu_fyi_windows_builder(
    name = 'Win10 FYI x64 SkiaRenderer Dawn Release (NVIDIA)',
)

ci.gpu_fyi_thin_tester(
    name = 'Win10 FYI x64 SkiaRenderer GL (NVIDIA)',
    triggered_by = ['GPU FYI Win x64 Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Win10 FYI x64 dEQP Release (Intel HD 630)',
    triggered_by = ['GPU FYI Win x64 dEQP Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Win10 FYI x64 dEQP Release (NVIDIA)',
    triggered_by = ['GPU FYI Win x64 dEQP Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Win10 FYI x86 Release (NVIDIA)',
    triggered_by = ['GPU FYI Win Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Win7 FYI Debug (AMD)',
    triggered_by = ['GPU FYI Win Builder (dbg)'],
)

ci.gpu_fyi_thin_tester(
    name = 'Win7 FYI Release (AMD)',
    triggered_by = ['GPU FYI Win Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Win7 FYI Release (NVIDIA)',
    triggered_by = ['GPU FYI Win Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Win7 FYI dEQP Release (AMD)',
    triggered_by = ['GPU FYI Win dEQP Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Win7 FYI x64 Release (NVIDIA)',
    triggered_by = ['GPU FYI Win x64 Builder'],
)

ci.gpu_fyi_thin_tester(
    name = 'Win7 FYI x64 dEQP Release (NVIDIA)',
    triggered_by = ['GPU FYI Win x64 dEQP Builder'],
)


ci.gpu_fyi_windows_builder(
    name = 'GPU FYI Win Builder',
)

ci.gpu_fyi_windows_builder(
    name = 'GPU FYI Win Builder (dbg)',
)

ci.gpu_fyi_windows_builder(
    name = 'GPU FYI Win dEQP Builder',
)

ci.gpu_fyi_windows_builder(
    name = 'GPU FYI Win x64 Builder',
)

ci.gpu_fyi_windows_builder(
    name = 'GPU FYI Win x64 Builder (dbg)',
)

ci.gpu_fyi_windows_builder(
    name = 'GPU FYI Win x64 dEQP Builder',
)

ci.gpu_fyi_windows_builder(
    name = 'GPU FYI Win x64 DX12 Vulkan Builder',
)

ci.gpu_fyi_windows_builder(
    name = 'GPU FYI Win x64 DX12 Vulkan Builder (dbg)',
)

ci.gpu_fyi_windows_builder(
    name = 'GPU FYI XR Win x64 Builder',
)

ci.linux_builder(
    name = 'Cast Audio Linux',
    ssd = True,
)

ci.linux_builder(
    name = 'Deterministic Fuchsia (dbg)',
    executable = 'recipe:swarming/deterministic_build',
    execution_timeout = 6 * time.hour,
    goma_jobs = None,
)

ci.linux_builder(
    name = 'Deterministic Linux',
    executable = 'recipe:swarming/deterministic_build',
    execution_timeout = 6 * time.hour,
)

ci.linux_builder(
    name = 'Deterministic Linux (dbg)',
    cores = 32,
    executable = 'recipe:swarming/deterministic_build',
    execution_timeout = 6 * time.hour,
)

ci.linux_builder(
    name = 'Leak Detection Linux',
)

ci.linux_builder(
    name = 'Linux Builder (dbg)(32)',
)

ci.linux_builder(
    name = 'Network Service Linux',
)

ci.linux_builder(
    name = 'fuchsia-x64-dbg',
    notifies = ['cr-fuchsia'],
)

ci.linux_builder(
    name = 'linux-gcc-rel',
    goma_backend = None,
)

ci.linux_builder(
    name = 'linux-trusty-rel',
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
    executable = 'recipe:chromium',
)

ci.mac_ios_builder(
    name = 'ios-simulator-full-configs',
)

ci.mac_ios_builder(
    name = 'ios-simulator-noncq',
)


ci.memory_builder(
    name = 'Android CFI',
    cores = 32,
    # TODO(https://crbug.com/919430) Remove the larger timeout once compile
    # times have been brought down to reasonable level
    execution_timeout = time.hour * 9 / 2,  # 4.5 (can't multiply float * duration)
)

ci.memory_builder(
    name = 'Linux CFI',
    cores = 32,
    # TODO(thakis): Remove once https://crbug.com/927738 is resolved.
    execution_timeout = 4 * time.hour,
    goma_jobs = goma.jobs.MANY_JOBS_FOR_CI,
)

ci.memory_builder(
    name = 'Linux Chromium OS ASan LSan Builder',
    # TODO(crbug.com/1030593): Builds take more than 3 hours sometimes. Remove
    # once the builds are faster.
    execution_timeout = 6 * time.hour,
)

ci.memory_builder(
    name = 'Linux Chromium OS ASan LSan Tests (1)',
    triggered_by = ['Linux Chromium OS ASan LSan Builder'],
)

ci.memory_builder(
    name = 'Linux ChromiumOS MSan Builder',
)

ci.memory_builder(
    name = 'Linux ChromiumOS MSan Tests',
    triggered_by = ['Linux ChromiumOS MSan Builder'],
)

ci.memory_builder(
    name = 'Linux MSan Builder',
    goma_jobs = goma.jobs.MANY_JOBS_FOR_CI,
)

ci.memory_builder(
    name = 'Linux MSan Tests',
    triggered_by = ['Linux MSan Builder'],
)

ci.memory_builder(
    name = 'Linux TSan Builder',
)

ci.memory_builder(
    name = 'Linux TSan Tests',
    triggered_by = ['Linux TSan Builder'],
)

ci.memory_builder(
    name = 'Mac ASan 64 Builder',
    builderless = False,
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
    os = os.MAC_DEFAULT,
    triggered_by = ['Mac ASan 64 Builder'],
)

ci.memory_builder(
    name = 'WebKit Linux ASAN',
)

ci.memory_builder(
    name = 'WebKit Linux Leak',
)

ci.memory_builder(
    name = 'WebKit Linux MSAN',
)

ci.memory_builder(
    name = 'android-asan',
)

ci.memory_builder(
    name = 'win-asan',
    cores = 32,
    builderless = True,
    os = os.WINDOWS_DEFAULT,
)


ci.swangle_linux_builder(
    name = 'linux-swangle-tot-angle-x64',
)

ci.swangle_linux_builder(
    name = 'linux-swangle-tot-angle-x86',
)

ci.swangle_linux_builder(
    name = 'linux-swangle-tot-swiftshader-x64',
)

ci.swangle_linux_builder(
    name = 'linux-swangle-tot-swiftshader-x86',
)

ci.swangle_linux_builder(
    name = 'linux-swangle-x64',
)

ci.swangle_linux_builder(
    name = 'linux-swangle-x86',
)


ci.swangle_windows_builder(
    name = 'win-swangle-tot-angle-x64',
)

ci.swangle_windows_builder(
    name = 'win-swangle-tot-angle-x86',
)

ci.swangle_windows_builder(
    name = 'win-swangle-tot-swiftshader-x64',
)

ci.swangle_windows_builder(
    name = 'win-swangle-tot-swiftshader-x86',
)

ci.swangle_windows_builder(
    name = 'win-swangle-x64',
)

ci.swangle_windows_builder(
    name = 'win-swangle-x86',
)


ci.win_builder(
    name = 'WebKit Win10',
    triggered_by = ['Win Builder'],
)

ci.win_builder(
    name = 'Win Builder',
    cores = 32,
    os = os.WINDOWS_ANY,
)

ci.win_builder(
    name = 'Win x64 Builder (dbg)',
    cores = 32,
    builderless = True,
    os = os.WINDOWS_ANY,
)

ci.win_builder(
    name = 'Win10 Tests x64 (dbg)',
    triggered_by = ['Win x64 Builder (dbg)'],
)

ci.win_builder(
    name = 'Win7 (32) Tests',
    os = os.WINDOWS_7,
    triggered_by = ['Win Builder'],
)

ci.win_builder(
    name = 'Win7 Tests (1)',
    os = os.WINDOWS_7,
    triggered_by = ['Win Builder'],
)

ci.win_builder(
    name = 'Windows deterministic',
    executable = 'recipe:swarming/deterministic_build',
    execution_timeout = 6 * time.hour,
)
