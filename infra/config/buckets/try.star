load('//lib/builders.star', 'cpu', 'goma', 'os')
load('//lib/try.star', 'try_')

# Defaults that apply to all branch versions of the bucket

try_.defaults.build_numbers.set(True)
try_.defaults.configure_kitchen.set(True)
try_.defaults.cores.set(8)
try_.defaults.cpu.set(cpu.X86_64)
try_.defaults.executable.set('recipe:chromium_trybot')
try_.defaults.execution_timeout.set(4 * time.hour)
# Max. pending time for builds. CQ considers builds pending >2h as timed
# out: http://shortn/_8PaHsdYmlq. Keep this in sync.
try_.defaults.expiration_timeout.set(2 * time.hour)
try_.defaults.os.set(os.LINUX_DEFAULT)
try_.defaults.service_account.set('chromium-try-builder@chops-service-accounts.iam.gserviceaccount.com')
try_.defaults.swarming_tags.set(['vpython:native-python-wrapper'])
try_.defaults.task_template_canary_percentage.set(5)

try_.defaults.caches.set([
    swarming.cache(
        name = 'win_toolchain',
        path = 'win_toolchain',
    ),
])


# Execute the versioned files to define all of the per-branch entities
# (bucket, builders, console, cq_group, etc.)
exec('//versioned/branches/beta/buckets/try.star')
exec('//versioned/branches/stable/buckets/try.star')
exec('//versioned/trunk/buckets/try.star')


# *** After this point everything is trunk only ***
try_.defaults.bucket.set('try')
try_.defaults.cq_group.set('cq')


# Builders are sorted first lexicographically by the function used to define
# them, then lexicographically by their name


try_.blink_builder(
    name = 'linux-blink-rel',
    goma_backend = goma.backend.RBE_PROD,
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/cc/.+',
            '.+/[+]/third_party/blink/renderer/core/paint/.+',
            '.+/[+]/third_party/blink/renderer/core/svg/.+',
            '.+/[+]/third_party/blink/renderer/platform/graphics/.+',
        ],
    ),
)

try_.blink_builder(
    name = 'win10-blink-rel',
    goma_backend = goma.backend.RBE_PROD,
    os = os.WINDOWS_ANY,
    builderless = True,
)

try_.blink_builder(
    name = 'win7-blink-rel',
    goma_backend = goma.backend.RBE_PROD,
    os = os.WINDOWS_ANY,
    builderless = True,
)


try_.blink_mac_builder(
    name = 'mac10.10-blink-rel',
)

try_.blink_mac_builder(
    name = 'mac10.11-blink-rel',
)

try_.blink_mac_builder(
    name = 'mac10.12-blink-rel',
)

try_.blink_mac_builder(
    name = 'mac10.13-blink-rel',
)

try_.blink_mac_builder(
    name = 'mac10.13_retina-blink-rel',
)

try_.blink_mac_builder(
    name = 'mac10.14-blink-rel',
)


try_.chromium_android_builder(
    name = 'android-asan',
)

try_.chromium_android_builder(
    name = 'android-bfcache-debug',
)

try_.chromium_android_builder(
    name = 'android-binary-size',
    executable = 'recipe:binary_size_trybot',
    goma_jobs = goma.jobs.J150,
    tryjob = try_.job(),
)

try_.chromium_android_builder(
    name = 'android-cronet-arm-dbg',
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/components/cronet/.+',
            '.+/[+]/components/grpc_support/.+',
            '.+/[+]/build/android/.+',
            '.+/[+]/build/config/android/.+',
        ],
        location_regexp_exclude = [
            '.+/[+]/components/cronet/ios/.+',
        ],
    ),
)

try_.chromium_android_builder(
    name = 'android-deterministic-dbg',
    executable = 'recipe:swarming/deterministic_build',
    execution_timeout = 6 * time.hour,
)

try_.chromium_android_builder(
    name = 'android-deterministic-rel',
    executable = 'recipe:swarming/deterministic_build',
    execution_timeout = 6 * time.hour,
)

try_.chromium_android_builder(
    name = 'android-lollipop-arm-rel',
)

try_.chromium_android_builder(
    name = 'android-marshmallow-x86-fyi-rel',
)

try_.chromium_android_builder(
    name = 'android-opus-kitkat-arm-rel',
)

try_.chromium_android_builder(
    name = 'android-oreo-arm64-cts-networkservice-dbg',
)

try_.chromium_android_builder(
    name = 'android-oreo-arm64-dbg',
)

try_.chromium_android_builder(
    name = 'android-pie-arm64-dbg',
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/chrome/android/features/vr/.+',
            '.+/[+]/chrome/android/java/src/org/chromium/chrome/browser/vr/.+',
            '.+/[+]/chrome/android/javatests/src/org/chromium/chrome/browser/vr/.+',
            '.+/[+]/chrome/browser/vr/.+',
            '.+/[+]/third_party/gvr-android-sdk/.+',
            '.+/[+]/third_party/arcore-android-sdk/.+',
            '.+/[+]/third_party/arcore-android-sdk-client/.+',
        ],
    ),
)

try_.chromium_android_builder(
    name = 'android-pie-x86-fyi-rel',
)

try_.chromium_android_builder(
    name = 'android-pie-arm64-coverage-rel',
    cores = 16,
    goma_jobs = goma.jobs.J300,
    ssd = True,
    use_clang_coverage = True,
)

try_.chromium_android_builder(
    name = 'android-pie-arm64-rel',
    cores = 16,
    goma_jobs = goma.jobs.J300,
    ssd = True,
)

try_.chromium_android_builder(
    name = 'android-10-arm64-rel',
)

try_.chromium_android_builder(
    name = 'android-webview-marshmallow-arm64-dbg',
)

try_.chromium_android_builder(
    name = 'android-webview-nougat-arm64-dbg',
)

try_.chromium_android_builder(
    name = 'android-webview-oreo-arm64-dbg',
)

try_.chromium_android_builder(
    name = 'android-webview-pie-arm64-dbg',
)

try_.chromium_android_builder(
    name = 'android-webview-pie-arm64-fyi-rel',
)

try_.chromium_android_builder(
    name = 'android_archive_rel_ng',
)

try_.chromium_android_builder(
    name = 'android_arm64_dbg_recipe',
    goma_jobs = goma.jobs.J300,
)

try_.chromium_android_builder(
    name = 'android_blink_rel',
)

try_.chromium_android_builder(
    name = 'android_cfi_rel_ng',
    cores = 32,
)

try_.chromium_android_builder(
    name = 'android_clang_dbg_recipe',
    goma_jobs = goma.jobs.J300,
)

try_.chromium_android_builder(
    name = 'android_compile_dbg',
    goma_jobs = goma.jobs.J150,
    tryjob = try_.job(),
)

try_.chromium_android_builder(
    name = 'android_compile_x64_dbg',
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/chrome/android/java/src/org/chromium/chrome/browser/vr/.+',
            '.+/[+]/chrome/browser/vr/.+',
            '.+/[+]/sandbox/linux/seccomp-bpf/.+',
            '.+/[+]/sandbox/linux/seccomp-bpf-helpers/.+',
            '.+/[+]/sandbox/linux/system_headers/.+',
            '.+/[+]/sandbox/linux/tests/.+',
            '.+/[+]/third_party/gvr-android-sdk/.+',
        ],
    ),
)

try_.chromium_android_builder(
    name = 'android_compile_x86_dbg',
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/chrome/android/java/src/org/chromium/chrome/browser/vr/.+',
            '.+/[+]/chrome/browser/vr/.+',
            '.+/[+]/sandbox/linux/seccomp-bpf/.+',
            '.+/[+]/sandbox/linux/seccomp-bpf-helpers/.+',
            '.+/[+]/sandbox/linux/system_headers/.+',
            '.+/[+]/sandbox/linux/tests/.+',
            '.+/[+]/third_party/gvr-android-sdk/.+',
        ],
    ),
)

try_.chromium_android_builder(
    name = 'android_cronet',
    tryjob = try_.job(),
)

try_.chromium_android_builder(
    name = 'android_cronet_tester',
    properties = {
        'buildername': 'android-cronet-arm-dbg',
    },
)

try_.chromium_android_builder(
    name = 'android_mojo',
)

try_.chromium_android_builder(
    name = 'android_n5x_swarming_dbg',
)

try_.chromium_android_builder(
    name = 'android_unswarmed_pixel_aosp',
)

try_.chromium_android_builder(
    name = 'cast_shell_android',
    tryjob = try_.job(),
)

try_.chromium_android_builder(
    name = 'linux_android_dbg_ng',
)

try_.chromium_android_builder(
    name = 'try-nougat-phone-tester',
)


try_.chromium_angle_builder(
    name = 'android_angle_deqp_rel_ng',
)

try_.chromium_angle_builder(
    name = 'android_angle_rel_ng',
)

try_.chromium_angle_builder(
    name = 'android_angle_vk32_deqp_rel_ng',
)

try_.chromium_angle_builder(
    name = 'android_angle_vk32_rel_ng',
)

try_.chromium_angle_builder(
    name = 'android_angle_vk64_deqp_rel_ng',
)

try_.chromium_angle_builder(
    name = 'android_angle_vk64_rel_ng',
)

try_.chromium_angle_builder(
    name = 'fuchsia-angle-rel',
)

try_.chromium_angle_builder(
    name = 'linux-angle-rel',
)

try_.chromium_angle_builder(
    name = 'linux_angle_deqp_rel_ng',
)

try_.chromium_angle_builder(
    name = 'linux_angle_ozone_rel_ng',
)

try_.chromium_angle_builder(
    name = 'mac-angle-rel',
    cores = None,
    os = os.MAC_ANY,
)

try_.chromium_angle_builder(
    name = 'win-angle-deqp-rel-32',
    os = os.WINDOWS_ANY,
)

try_.chromium_angle_builder(
    name = 'win-angle-deqp-rel-64',
    os = os.WINDOWS_ANY,
)

try_.chromium_angle_builder(
    name = 'win-angle-rel-32',
    os = os.WINDOWS_ANY,
)

try_.chromium_angle_builder(
    name = 'win-angle-rel-64',
    os = os.WINDOWS_ANY,
)


try_.chromium_chromiumos_builder(
    name = 'chromeos-amd64-generic-dbg',
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/content/gpu/.+',
            '.+/[+]/media/.+',
        ],
    ),
)

try_.chromium_chromiumos_builder(
    name = 'chromeos-amd64-generic-cfi-thin-lto-rel',
)

try_.chromium_chromiumos_builder(
    name = 'chromeos-arm-generic-dbg',
)

try_.chromium_chromiumos_builder(
    name = 'chromeos-arm-generic-rel',
    tryjob = try_.job(),
)

try_.chromium_chromiumos_builder(
    name = 'chromeos-kevin-compile-rel',
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/chromeos/CHROMEOS_LKGM',
        ],
    ),
)

try_.chromium_chromiumos_builder(
    name = 'chromeos-kevin-rel',
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/build/chromeos/.+',
            '.+/[+]/build/config/chromeos/.*',
        ],
    ),
)

try_.chromium_chromiumos_builder(
    name = 'linux-chromeos-compile-dbg',
    tryjob = try_.job(),
)

try_.chromium_chromiumos_builder(
    name = 'linux-chromeos-dbg',
)


try_.chromium_dawn_builder(
    name = 'dawn-linux-x64-deps-rel',
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/gpu/.+',
            '.+/[+]/testing/buildbot/chromium.dawn.json',
            '.+/[+]/third_party/blink/renderer/modules/webgpu/.+',
            '.+/[+]/third_party/blink/web_tests/external/wpt/webgpu/.+',
            '.+/[+]/third_party/blink/web_tests/wpt_internal/webgpu/.+',
            '.+/[+]/third_party/blink/web_tests/WebGPUExpectations',
            '.+/[+]/third_party/dawn/.+',
            '.+/[+]/tools/clang/scripts/update.py',
            '.+/[+]/ui/gl/features.gni',
        ],
    ),
)

try_.chromium_dawn_builder(
    name = 'dawn-mac-x64-deps-rel',
    os = os.MAC_ANY,
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/gpu/.+',
            '.+/[+]/testing/buildbot/chromium.dawn.json',
            '.+/[+]/third_party/blink/renderer/modules/webgpu/.+',
            '.+/[+]/third_party/blink/web_tests/external/wpt/webgpu/.+',
            '.+/[+]/third_party/blink/web_tests/wpt_internal/webgpu/.+',
            '.+/[+]/third_party/blink/web_tests/WebGPUExpectations',
            '.+/[+]/third_party/dawn/.+',
            '.+/[+]/tools/clang/scripts/update.py',
            '.+/[+]/ui/gl/features.gni',
        ],
    ),
)

try_.chromium_dawn_builder(
    name = 'dawn-win10-x64-deps-rel',
    os = os.WINDOWS_ANY,
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/gpu/.+',
            '.+/[+]/testing/buildbot/chromium.dawn.json',
            '.+/[+]/third_party/blink/renderer/modules/webgpu/.+',
            '.+/[+]/third_party/blink/web_tests/external/wpt/webgpu/.+',
            '.+/[+]/third_party/blink/web_tests/wpt_internal/webgpu/.+',
            '.+/[+]/third_party/blink/web_tests/WebGPUExpectations',
            '.+/[+]/third_party/dawn/.+',
            '.+/[+]/tools/clang/scripts/update.py',
            '.+/[+]/ui/gl/features.gni',
        ],
    ),
)

try_.chromium_dawn_builder(
    name = 'dawn-win10-x86-deps-rel',
    os = os.WINDOWS_ANY,
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/gpu/.+',
            '.+/[+]/testing/buildbot/chromium.dawn.json',
            '.+/[+]/third_party/blink/renderer/modules/webgpu/.+',
            '.+/[+]/third_party/blink/web_tests/external/wpt/webgpu/.+',
            '.+/[+]/third_party/blink/web_tests/wpt_internal/webgpu/.+',
            '.+/[+]/third_party/blink/web_tests/WebGPUExpectations',
            '.+/[+]/third_party/dawn/.+',
            '.+/[+]/tools/clang/scripts/update.py',
            '.+/[+]/ui/gl/features.gni',
        ],
    ),
)

try_.chromium_dawn_builder(
    name = 'linux-dawn-rel',
)

try_.chromium_dawn_builder(
    name = 'mac-dawn-rel',
    os = os.MAC_ANY,
)

try_.chromium_dawn_builder(
    name = 'win-dawn-rel',
    os = os.WINDOWS_ANY,
)


try_.chromium_linux_builder(
    name = 'cast_shell_audio_linux',
)

try_.chromium_linux_builder(
    name = 'cast_shell_linux',
    tryjob = try_.job(),
)

try_.chromium_linux_builder(
    name = 'closure_compilation',
    executable = 'recipe:closure_compilation',
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/third_party/closure_compiler/.+',
        ],
    ),
)

try_.chromium_linux_builder(
    name = 'fuchsia-arm64-cast',
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/chromecast/.+',
        ],
    ),
)

try_.chromium_linux_builder(
    name = 'fuchsia-compile-x64-dbg',
    tryjob = try_.job(
        experiment_percentage = 50,
    ),
)

try_.chromium_linux_builder(
    name = 'fuchsia-fyi-arm64-rel',
)

try_.chromium_linux_builder(
    name = 'fuchsia-fyi-x64-dbg',
)

try_.chromium_linux_builder(
    name = 'fuchsia-fyi-x64-rel',
)

try_.chromium_linux_builder(
    name = 'fuchsia-x64-cast',
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/chromecast/.+',
        ],
    ),
)

try_.chromium_linux_builder(
    name = 'fuchsia_arm64',
    tryjob = try_.job(),
)

try_.chromium_linux_builder(
    name = 'fuchsia_x64',
    tryjob = try_.job(),
)

try_.chromium_linux_builder(
    name = 'layout_test_leak_detection',
)

try_.chromium_linux_builder(
    name = 'leak_detection_linux',
)

try_.chromium_linux_builder(
    name = 'linux-annotator-rel',
)

try_.chromium_linux_builder(
    name = 'linux-bfcache-debug',
)

try_.chromium_linux_builder(
    name = 'linux-blink-heap-concurrent-marking-tsan-rel',
)

try_.chromium_linux_builder(
    name = 'linux-blink-heap-verification-try',
)

try_.chromium_linux_builder(
    name = 'linux-clang-tidy-dbg',
    executable = 'recipe:tricium_clang_tidy_wrapper',
    goma_jobs = goma.jobs.J150,
)

try_.chromium_linux_builder(
    name = 'linux-clang-tidy-rel',
    executable = 'recipe:tricium_clang_tidy_wrapper',
    goma_jobs = goma.jobs.J150,
)

try_.chromium_linux_builder(
    name = 'linux-dcheck-off-rel',
)

try_.chromium_linux_builder(
    name = 'linux-gcc-rel',
    goma_backend = None,
)

try_.chromium_linux_builder(
    name = 'linux-libfuzzer-asan-rel',
    executable = 'recipe:chromium_libfuzzer_trybot',
    tryjob = try_.job(),
)

try_.chromium_linux_builder(
    name = 'linux-ozone-rel',
    tryjob = try_.job(),
)

try_.chromium_linux_builder(
    name = 'linux-trusty-rel',
    goma_jobs = goma.jobs.J150,
    os = os.LINUX_TRUSTY,
)

try_.chromium_linux_builder(
    name = 'linux-viz-rel',
)

try_.chromium_linux_builder(
    name = 'linux-webkit-msan-rel',
)

try_.chromium_linux_builder(
    name = 'linux_arm',
)

try_.chromium_linux_builder(
    name = 'linux_chromium_analysis',
)

try_.chromium_linux_builder(
    name = 'linux_chromium_archive_rel_ng',
)

try_.chromium_linux_builder(
    name = 'linux_chromium_asan_rel_ng',
    goma_jobs = goma.jobs.J150,
    ssd = True,
    tryjob = try_.job(),
)

try_.chromium_linux_builder(
    name = 'linux_chromium_cfi_rel_ng',
    cores = 32,
)

try_.chromium_linux_builder(
    name = 'linux_chromium_chromeos_asan_rel_ng',
    goma_jobs = goma.jobs.J150,
)

try_.chromium_linux_builder(
    name = 'linux_chromium_chromeos_msan_rel_ng',
    goma_jobs = goma.jobs.J150,
)

try_.chromium_linux_builder(
    name = 'linux_chromium_clobber_deterministic',
    executable = 'recipe:swarming/deterministic_build',
    execution_timeout = 6 * time.hour,
)

try_.chromium_linux_builder(
    name = 'linux_chromium_clobber_rel_ng',
)

try_.chromium_linux_builder(
    name = 'linux_chromium_compile_dbg_32_ng',
)

try_.chromium_linux_builder(
    name = 'linux_chromium_compile_dbg_ng',
    caches = [
        swarming.cache(
            name = 'builder',
            path = 'linux_debug',
        ),
    ],
    goma_jobs = goma.jobs.J150,
    tryjob = try_.job(),
)

try_.chromium_linux_builder(
    name = 'linux_chromium_compile_rel_ng',
)

try_.chromium_linux_builder(
    name = 'linux_chromium_dbg_ng',
    caches = [
        swarming.cache(
            name = 'builder',
            path = 'linux_debug',
        ),
    ],
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/build/.*check_gn_headers.*',
        ],
    ),
)

try_.chromium_linux_builder(
    name = 'linux_chromium_msan_rel_ng',
    goma_jobs = goma.jobs.J150,
)

try_.chromium_linux_builder(
    name = 'linux_chromium_tsan_rel_ng',
    goma_jobs = goma.jobs.J150,
    tryjob = try_.job(),
)

try_.chromium_linux_builder(
    name = 'linux_chromium_ubsan_rel_ng',
)

try_.chromium_linux_builder(
    name = 'linux_layout_tests_composite_after_paint',
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/third_party/blink/renderer/core/paint/.+',
            '.+/[+]/third_party/blink/renderer/core/svg/.+',
            '.+/[+]/third_party/blink/renderer/platform/graphics/.+',
            '.+/[+]/third_party/blink/web_tests/FlagExpectations/composite-after-paint',
            '.+/[+]/third_party/blink/web_tests/flag-specific/composite-after-paint/.+',
        ],
    ),
)

try_.chromium_linux_builder(
    name = 'linux_layout_tests_layout_ng_disabled',
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/third_party/blink/renderer/core/editing/.+',
            '.+/[+]/third_party/blink/renderer/core/layout/.+',
            '.+/[+]/third_party/blink/renderer/core/paint/.+',
            '.+/[+]/third_party/blink/renderer/core/svg/.+',
            '.+/[+]/third_party/blink/renderer/platform/fonts/shaping/.+',
            '.+/[+]/third_party/blink/renderer/platform/graphics/.+',
            '.+/[+]/third_party/blink/web_tests/FlagExpectations/disable-layout-ng',
            '.+/[+]/third_party/blink/web_tests/flag-specific/disable-layout-ng/.+',
        ],
    ),
)

try_.chromium_linux_builder(
    name = 'linux-layout-tests-fragment-item',
)

try_.chromium_linux_builder(
    name = 'linux-layout-tests-fragment-paint',
)

try_.chromium_linux_builder(
    name = 'linux_mojo',
)

try_.chromium_linux_builder(
    name = 'linux_mojo_chromeos',
)

try_.chromium_linux_builder(
    name = 'linux_upload_clang',
    builderless = True,
    cores = 32,
    executable = 'recipe:chromium_upload_clang',
    goma_backend = None,
    os = os.LINUX_TRUSTY,
)

try_.chromium_linux_builder(
    name = 'linux_vr',
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/chrome/browser/vr/.+',
        ],
    ),
)

try_.chromium_linux_builder(
    name = 'linux-wpt-fyi-rel',
)

try_.chromium_linux_builder(
    name = 'tricium-metrics-analysis',
    executable = 'recipe:tricium_metrics',
)


try_.chromium_mac_builder(
    name = 'mac-osxbeta-rel',
    os = os.MAC_DEFAULT,
)

# NOTE: the following 4 trybots aren't sensitive to Mac version on which
# they are built, hence no additional dimension is specified.
# The 10.xx version translates to which bots will run isolated tests.
try_.chromium_mac_builder(
    name = 'mac_chromium_10.10',
)

try_.chromium_mac_builder(
    name = 'mac_chromium_10.12_rel_ng',
)

try_.chromium_mac_builder(
    name = 'mac_chromium_10.13_rel_ng',
)

try_.chromium_mac_builder(
    name = 'mac_chromium_10.14_rel_ng',
)

try_.chromium_mac_builder(
    name = 'mac_chromium_archive_rel_ng',
)

try_.chromium_mac_builder(
    name = 'mac_chromium_asan_rel_ng',
    goma_jobs = goma.jobs.J150,
)

try_.chromium_mac_builder(
    name = 'mac_chromium_compile_dbg_ng',
    goma_jobs = goma.jobs.J150,
    os = os.MAC_10_13,
    tryjob = try_.job(),
)

try_.chromium_mac_builder(
    name = 'mac_chromium_compile_rel_ng',
)

try_.chromium_mac_builder(
    name = 'mac_chromium_dbg_ng',
)

try_.chromium_mac_builder(
    name = 'mac_upload_clang',
    builderless = False,
    caches = [
        swarming.cache(
            name = 'xcode_mac_9a235',
            path = 'xcode_mac_9a235.app',
        ),
    ],
    executable = 'recipe:chromium_upload_clang',
    execution_timeout = 6 * time.hour,
    goma_backend = None,  # Does not use Goma.
    properties = {
        '$depot_tools/osx_sdk': {
            'sdk_version': '9a235',
        },
    },
)


try_.chromium_mac_ios_builder(
    name = 'ios-device',
)

try_.chromium_mac_ios_builder(
    name = 'ios-device-xcode-clang',
)

try_.chromium_mac_ios_builder(
    name = 'ios-simulator-cr-recipe',
    executable = 'recipe:chromium_trybot',
    properties = {
        'xcode_build_version': '11a1027',
    },
)

try_.chromium_mac_ios_builder(
    name = 'ios-simulator-cronet',
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/components/cronet/.+',
            '.+/[+]/components/grpc_support/.+',
            '.+/[+]/ios/.+',
        ],
        location_regexp_exclude = [
            '.+/[+]/components/cronet/android/.+',
        ],
    ),
)

try_.chromium_mac_ios_builder(
    name = 'ios-simulator-eg',
)

try_.chromium_mac_ios_builder(
    name = 'ios-simulator-noncq',
)

try_.chromium_mac_ios_builder(
    name = 'ios-simulator-full-configs',
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/ios/.+',
        ],
    ),
)

try_.chromium_mac_ios_builder(
    name = 'ios-simulator-xcode-clang',
)

try_.chromium_mac_ios_builder(
    name = 'ios13-beta-simulator',
)

try_.chromium_mac_ios_builder(
    name = 'ios13-sdk-simulator',
)



try_.chromium_swangle_linux_builder(
    name = 'linux-swangle-try-tot-angle-x64'
)

try_.chromium_swangle_linux_builder(
    name = 'linux-swangle-try-tot-angle-x86'
)

try_.chromium_swangle_linux_builder(
    name = 'linux-swangle-try-tot-swiftshader-x64'
)

try_.chromium_swangle_linux_builder(
    name = 'linux-swangle-try-tot-swiftshader-x86'
)

try_.chromium_swangle_linux_builder(
    name = 'linux-swangle-try-x64'
)

try_.chromium_swangle_linux_builder(
    name = 'linux-swangle-try-x86'
)


try_.chromium_swangle_windows_builder(
    name = 'win-swangle-try-tot-angle-x64',
)

try_.chromium_swangle_windows_builder(
    name = 'win-swangle-try-tot-angle-x86',
)

try_.chromium_swangle_windows_builder(
    name = 'win-swangle-try-tot-swiftshader-x64',
)

try_.chromium_swangle_windows_builder(
    name = 'win-swangle-try-tot-swiftshader-x86',
)

try_.chromium_swangle_windows_builder(
    name = 'win-swangle-try-x64',
)

try_.chromium_swangle_windows_builder(
    name = 'win-swangle-try-x86',
)


try_.chromium_win_builder(
    name = 'win-annotator-rel',
)

try_.chromium_win_builder(
    name = 'win-asan',
    goma_jobs = goma.jobs.J150,
)

try_.chromium_win_builder(
    name = 'win-celab-try-rel',
    executable = 'recipe:celab',
    properties = {
        'exclude': 'chrome_only',
        'pool_name': 'celab-chromium-try',
        'pool_size': 20,
        'tests': '*',
    },
)

try_.chromium_win_builder(
    name = 'win-libfuzzer-asan-rel',
    builderless = False,
    executable = 'recipe:chromium_libfuzzer_trybot',
    os = os.WINDOWS_ANY,
    tryjob = try_.job(),
)

try_.chromium_win_builder(
    name = 'win10_chromium_x64_dbg_ng',
    os = os.WINDOWS_10,
)

try_.chromium_win_builder(
    name = 'win10_chromium_x64_coverage_rel_ng',
    os = os.WINDOWS_10,
    use_clang_coverage = True,
    goma_jobs = goma.jobs.J150,
    ssd = True,
    tryjob = try_.job(experiment_percentage = 3),
)

try_.chromium_win_builder(
    name = 'win10_chromium_x64_rel_ng_exp',
    builderless = False,
    os = os.WINDOWS_ANY,
)

try_.chromium_win_builder(
    name = 'win7-rel',
    execution_timeout = time.hour * 9 / 2,  # 4.5 (can't multiply float * duration)
    goma_jobs = goma.jobs.J300,
    ssd = True,
)

try_.chromium_win_builder(
    name = 'win_archive',
)

try_.chromium_win_builder(
    name = 'win_chromium_compile_dbg_ng',
    goma_jobs = goma.jobs.J150,
    tryjob = try_.job(),
)

try_.chromium_win_builder(
    name = 'win_chromium_compile_rel_ng',
)

try_.chromium_win_builder(
    name = 'win_chromium_dbg_ng',
)

try_.chromium_win_builder(
    name = 'win_chromium_x64_rel_ng',
)

try_.chromium_win_builder(
    name = 'win_mojo',
)

try_.chromium_win_builder(
    name = 'win_upload_clang',
    builderless = False,
    cores = 32,
    executable = 'recipe:chromium_upload_clang',
    goma_backend = None,
    os = os.WINDOWS_ANY,
)

try_.chromium_win_builder(
    name = 'win_x64_archive',
)


try_.gpu_chromium_android_builder(
    name = 'android_optional_gpu_tests_rel',
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/cc/.+',
            '.+/[+]/chrome/browser/vr/.+',
            '.+/[+]/components/viz/.+',
            '.+/[+]/content/test/gpu/.+',
            '.+/[+]/gpu/.+',
            '.+/[+]/media/audio/.+',
            '.+/[+]/media/filters/.+',
            '.+/[+]/media/gpu/.+',
            '.+/[+]/services/viz/.+',
            '.+/[+]/testing/trigger_scripts/.+',
            '.+/[+]/third_party/blink/renderer/modules/webgl/.+',
            '.+/[+]/third_party/blink/renderer/platform/graphics/gpu/.+',
            '.+/[+]/tools/clang/scripts/update.py',
            '.+/[+]/ui/gl/.+',
        ],
    ),
)


try_.gpu_chromium_linux_builder(
    name = 'linux_optional_gpu_tests_rel',
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/chrome/browser/vr/.+',
            '.+/[+]/content/test/gpu/.+',
            '.+/[+]/gpu/.+',
            '.+/[+]/media/audio/.+',
            '.+/[+]/media/filters/.+',
            '.+/[+]/media/gpu/.+',
            '.+/[+]/testing/buildbot/chromium.gpu.fyi.json',
            '.+/[+]/testing/trigger_scripts/.+',
            '.+/[+]/third_party/blink/renderer/modules/webgl/.+',
            '.+/[+]/third_party/blink/renderer/platform/graphics/gpu/.+',
            '.+/[+]/tools/clang/scripts/update.py',
            '.+/[+]/ui/gl/.+',
        ],
    ),
)


try_.gpu_chromium_mac_builder(
    name = 'mac_optional_gpu_tests_rel',
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/chrome/browser/vr/.+',
            '.+/[+]/content/test/gpu/.+',
            '.+/[+]/gpu/.+',
            '.+/[+]/media/audio/.+',
            '.+/[+]/media/filters/.+',
            '.+/[+]/media/gpu/.+',
            '.+/[+]/services/shape_detection/.+',
            '.+/[+]/testing/buildbot/chromium.gpu.fyi.json',
            '.+/[+]/testing/trigger_scripts/.+',
            '.+/[+]/third_party/blink/renderer/modules/webgl/.+',
            '.+/[+]/third_party/blink/renderer/platform/graphics/gpu/.+',
            '.+/[+]/tools/clang/scripts/update.py',
            '.+/[+]/ui/gl/.+',
        ],
    ),
)


try_.gpu_chromium_win_builder(
    name = 'win_optional_gpu_tests_rel',
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/chrome/browser/vr/.+',
            '.+/[+]/content/test/gpu/.+',
            '.+/[+]/device/vr/.+',
            '.+/[+]/gpu/.+',
            '.+/[+]/media/audio/.+',
            '.+/[+]/media/filters/.+',
            '.+/[+]/media/gpu/.+',
            '.+/[+]/testing/buildbot/chromium.gpu.fyi.json',
            '.+/[+]/testing/trigger_scripts/.+',
            '.+/[+]/third_party/blink/renderer/modules/vr/.+',
            '.+/[+]/third_party/blink/renderer/modules/webgl/.+',
            '.+/[+]/third_party/blink/renderer/modules/xr/.+',
            '.+/[+]/third_party/blink/renderer/platform/graphics/gpu/.+',
            '.+/[+]/tools/clang/scripts/update.py',
            '.+/[+]/ui/gl/.+',
        ],
    ),
)


# Used for listing chrome trybots in chromium's commit-queue.cfg without also
# adding them to chromium's cr-buildbucket.cfg. Note that the recipe these
# builders run allow only known roller accounts when triggered via the CQ.
def chrome_internal_verifier(
    *,
    builder):
  luci.cq_tryjob_verifier(
      builder = 'chrome:try/' + builder,
      cq_group = 'cq',
      includable_only = True,
      owner_whitelist = ["googlers"],
  )

chrome_internal_verifier(
    builder = 'chromeos-betty-chrome',
)

chrome_internal_verifier(
    builder = 'chromeos-betty-pi-arc-chrome',
)

chrome_internal_verifier(
    builder = 'chromeos-eve-compile-chrome',
)

chrome_internal_verifier(
    builder = 'chromeos-kevin-compile-chrome',
)

chrome_internal_verifier(
    builder = 'ipad-device',
)

chrome_internal_verifier(
    builder = 'iphone-device',
)

chrome_internal_verifier(
    builder = 'linux-chromeos-chrome',
)
