luci.cq_group(
    name = 'cq',
    # TODO(crbug/959436): enable it.
    cancel_stale_tryjobs = False,
    retry_config = cq.RETRY_ALL_FAILURES,
    tree_status_host = 'chromium-status.appspot.com/',
    watch = cq.refset(
        repo = 'https://chromium.googlesource.com/chromium/src',
        refs = ['refs/heads/.+'],
    ),
    acls = [
        acl.entry(
            acl.CQ_COMMITTER,
            groups = 'project-chromium-committers',
        ),
        acl.entry(
            acl.CQ_DRY_RUNNER,
            groups = 'project-chromium-tryjob-access',
        ),
    ],
    verifiers = [

        #############################
        # Always required builders. #
        #############################

        luci.cq_tryjob_verifier(builder = 'try/android-binary-size'),
        luci.cq_tryjob_verifier(builder = 'try/android-kitkat-arm-rel'),
        luci.cq_tryjob_verifier(builder = 'try/android-marshmallow-arm64-rel'),
        luci.cq_tryjob_verifier(builder = 'try/android_arm64_dbg_recipe'),
        luci.cq_tryjob_verifier(builder = 'try/android_clang_dbg_recipe'),
        luci.cq_tryjob_verifier(builder = 'try/android_compile_dbg'),
        luci.cq_tryjob_verifier(builder = 'try/android_cronet'),
        luci.cq_tryjob_verifier(builder = 'try/cast_shell_android'),
        luci.cq_tryjob_verifier(builder = 'try/cast_shell_linux'),
        luci.cq_tryjob_verifier(builder = 'try/chromeos-amd64-generic-rel'),
        luci.cq_tryjob_verifier(builder = 'try/chromeos-arm-generic-rel'),
        luci.cq_tryjob_verifier(
            builder = 'try/chromium_presubmit',
            disable_reuse = True,
        ),
        luci.cq_tryjob_verifier(builder = 'try/fuchsia_arm64'),
        luci.cq_tryjob_verifier(builder = 'try/fuchsia_x64'),
        luci.cq_tryjob_verifier(builder = 'try/ios-simulator'),
        luci.cq_tryjob_verifier(builder = 'try/linux-chromeos-compile-dbg'),
        luci.cq_tryjob_verifier(builder = 'try/linux-chromeos-rel'),
        luci.cq_tryjob_verifier(builder = 'try/linux-libfuzzer-asan-rel'),
        luci.cq_tryjob_verifier(builder = 'try/linux-ozone-rel'),
        luci.cq_tryjob_verifier(builder = 'try/linux-rel'),
        luci.cq_tryjob_verifier(builder = 'try/linux_chromium_asan_rel_ng'),
        luci.cq_tryjob_verifier(builder = 'try/linux_chromium_compile_dbg_ng'),
        luci.cq_tryjob_verifier(builder = 'try/linux_chromium_tsan_rel_ng'),
        luci.cq_tryjob_verifier(builder = 'try/mac-rel'),
        luci.cq_tryjob_verifier(builder = 'try/mac_chromium_compile_dbg_ng'),
        luci.cq_tryjob_verifier(builder = 'try/win-libfuzzer-asan-rel'),
        luci.cq_tryjob_verifier(builder = 'try/win10_chromium_x64_rel_ng'),
        luci.cq_tryjob_verifier(builder = 'try/win_chromium_compile_dbg_ng'),

        ######################
        # Optional builders. #
        ######################

        luci.cq_tryjob_verifier(
            builder = 'try/android-cronet-arm-dbg',
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
        luci.cq_tryjob_verifier(
            builder = 'try/android_compile_x64_dbg',
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
        luci.cq_tryjob_verifier(
            builder = 'try/android_compile_x86_dbg',
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
        luci.cq_tryjob_verifier(
            builder = 'try/android_optional_gpu_tests_rel',
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
                '.+/[+]/ui/gl/.+',
            ],
        ),
        luci.cq_tryjob_verifier(
            builder = 'try/chromeos-amd64-generic-dbg',
            location_regexp = [
                '.+/[+]/content/gpu/.+',
                '.+/[+]/media/.+',
            ],
        ),
        luci.cq_tryjob_verifier(
            builder = 'try/chromeos-kevin-compile-rel',
            location_regexp = [
                '.+/[+]/chromeos/CHROMEOS_LKGM',
            ],
        ),
        luci.cq_tryjob_verifier(
            builder = 'try/chromeos-kevin-rel',
            location_regexp = [
                '.+/[+]/build/chromeos/.+',
                '.+/[+]/build/config/chromeos/.*',
            ],
        ),
        luci.cq_tryjob_verifier(
            builder = 'try/closure_compilation',
            location_regexp = [
                '.+/[+]/third_party/closure_compiler/.+',
            ],
        ),
        luci.cq_tryjob_verifier(
            builder = 'try/dawn-linux-x64-deps-rel',
            location_regexp = [
                '.+/[+]/gpu/.+',
                '.+/[+]/testing/buildbot/chromium.dawn.json',
                '.+/[+]/third_party/blink/renderer/modules/webgpu/.+',
                '.+/[+]/third_party/blink/web_tests/external/wpt/webgpu/.+',
                '.+/[+]/third_party/blink/web_tests/wpt_internal/webgpu/.+',
                '.+/[+]/third_party/blink/web_tests/WebGPUExpectations',
                '.+/[+]/third_party/dawn/.+',
            ],
        ),
        luci.cq_tryjob_verifier(
            builder = 'try/dawn-mac-x64-deps-rel',
            location_regexp = [
                '.+/[+]/gpu/.+',
                '.+/[+]/testing/buildbot/chromium.dawn.json',
                '.+/[+]/third_party/blink/renderer/modules/webgpu/.+',
                '.+/[+]/third_party/blink/web_tests/external/wpt/webgpu/.+',
                '.+/[+]/third_party/blink/web_tests/wpt_internal/webgpu/.+',
                '.+/[+]/third_party/blink/web_tests/WebGPUExpectations',
                '.+/[+]/third_party/dawn/.+',
            ],
        ),
        luci.cq_tryjob_verifier(
            builder = 'try/dawn-win10-x64-deps-rel',
            location_regexp = [
                '.+/[+]/gpu/.+',
                '.+/[+]/testing/buildbot/chromium.dawn.json',
                '.+/[+]/third_party/blink/renderer/modules/webgpu/.+',
                '.+/[+]/third_party/blink/web_tests/external/wpt/webgpu/.+',
                '.+/[+]/third_party/blink/web_tests/wpt_internal/webgpu/.+',
                '.+/[+]/third_party/blink/web_tests/WebGPUExpectations',
                '.+/[+]/third_party/dawn/.+',
            ],
        ),
        luci.cq_tryjob_verifier(
            builder = 'try/dawn-win10-x86-deps-rel',
            location_regexp = [
                '.+/[+]/gpu/.+',
                '.+/[+]/testing/buildbot/chromium.dawn.json',
                '.+/[+]/third_party/blink/renderer/modules/webgpu/.+',
                '.+/[+]/third_party/blink/web_tests/external/wpt/webgpu/.+',
                '.+/[+]/third_party/blink/web_tests/wpt_internal/webgpu/.+',
                '.+/[+]/third_party/blink/web_tests/WebGPUExpectations',
                '.+/[+]/third_party/dawn/.+',
            ],
        ),
        luci.cq_tryjob_verifier(
            builder = 'try/fuchsia-arm64-cast',
            location_regexp = [
                '.+/[+]/chromecast/.+',
            ],
        ),
        luci.cq_tryjob_verifier(
            builder = 'try/fuchsia-x64-cast',
            location_regexp = [
                '.+/[+]/chromecast/.+',
            ],
        ),
        luci.cq_tryjob_verifier(
            builder = 'try/gpu-fyi-try-android-p-pixel-2-skv-32',
            # Some locations disabled due to limited capacity.
            location_regexp = [
                #'.+/[+]/cc/.+',
                '.+/[+]/components/viz/.+',
                '.+/[+]/content/test/gpu/gpu_tests/.+py',
                '.+/[+]/content/test/gpu/gpu_tests/test_expectations/pixel_expectations.txt',
                '.+/[+]/gpu/vulkan/.+',
                #'.+/[+]/media/gpu/.+',
                '.+/[+]/services/viz/.+',
                #'.+/[+]/third_party/blink/renderer/platform/graphics/gpu/.+',
                #'.+/[+]/third_party/skia/src/gpu/.+',
                #'.+/[+]/third_party/skia/include/gpu/.+',
            ],
        ),
        luci.cq_tryjob_verifier(
            builder = 'try/ios-simulator-cronet',
            location_regexp = [
                '.+/[+]/components/cronet/.+',
                '.+/[+]/components/grpc_support/.+',
                '.+/[+]/ios/.+',
            ],
            location_regexp_exclude = [
                '.+/[+]/components/cronet/android/.+',
            ],
        ),
        luci.cq_tryjob_verifier(
            builder = 'try/ios-simulator-full-configs',
            location_regexp = [
                '.+/[+]/ios/.+',
            ],
        ),
        luci.cq_tryjob_verifier(
            builder = 'try/linux-blink-rel',
            location_regexp = [
                '.+/[+]/cc/.+',
                '.+/[+]/third_party/blink/renderer/core/paint/.+',
                '.+/[+]/third_party/blink/renderer/core/svg/.+',
                '.+/[+]/third_party/blink/renderer/platform/graphics/.+',
                '.+/[+]/third_party/blink/web_tests/FlagExpectations/enable-blink-features=CompositeAfterPaint',
                '.+/[+]/third_party/blink/web_tests/flag-specific/enable-blink-features=CompositeAfterPaint/.+',
            ],
        ),
        luci.cq_tryjob_verifier(
            builder = 'try/linux_chromium_dbg_ng',
            location_regexp = [
                '.+/[+]/build/.*check_gn_headers.*',
            ],
        ),
        luci.cq_tryjob_verifier(
            builder = 'try/linux_layout_tests_composite_after_paint',
            location_regexp = [
                '.+/[+]/third_party/blink/renderer/core/paint/.+',
                '.+/[+]/third_party/blink/renderer/core/svg/.+',
                '.+/[+]/third_party/blink/renderer/platform/graphics/.+',
                '.+/[+]/third_party/blink/web_tests/FlagExpectations/enable-blink-features=CompositeAfterPaint',
                '.+/[+]/third_party/blink/web_tests/flag-specific/enable-blink-features=CompositeAfterPaint/.+',
            ],
        ),
        luci.cq_tryjob_verifier(
            builder = 'try/linux_layout_tests_layout_ng_disabled',
            location_regexp = [
                '.+/[+]/third_party/blink/renderer/core/editing/.+',
                '.+/[+]/third_party/blink/renderer/core/layout/.+',
                '.+/[+]/third_party/blink/renderer/core/paint/.+',
                '.+/[+]/third_party/blink/renderer/core/svg/.+',
                '.+/[+]/third_party/blink/renderer/platform/fonts/shaping/.+',
                '.+/[+]/third_party/blink/renderer/platform/graphics/.+',
                '.+/[+]/third_party/blink/web_tests/flag-specific/enable-blink-features=LayoutNG/.+',
            ],
        ),
        luci.cq_tryjob_verifier(
            builder = 'try/linux_optional_gpu_tests_rel',
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
                '.+/[+]/ui/gl/.+',
            ],
        ),
        luci.cq_tryjob_verifier(
            builder = 'try/linux_vr',
            location_regexp = [
                '.+/[+]/chrome/browser/vr/.+',
            ],
        ),
        luci.cq_tryjob_verifier(
            builder = 'try/mac_optional_gpu_tests_rel',
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
                '.+/[+]/ui/gl/.+',
            ],
        ),
        luci.cq_tryjob_verifier(
            builder = 'try/win_optional_gpu_tests_rel',
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
                '.+/[+]/ui/gl/.+',
            ],
        ),

        ##########################
        # Experimental builders. #
        ##########################

        luci.cq_tryjob_verifier(
            builder = 'try/android-marshmallow-arm64-coverage-rel',
            experiment_percentage = 20,
        ),
        luci.cq_tryjob_verifier(
            builder = 'try/android-pie-arm64-rel',
            experiment_percentage = 50,
        ),
        luci.cq_tryjob_verifier(
            builder = 'try/chromeos-kevin-experimental-rel',
            experiment_percentage = 5,
        ),
        luci.cq_tryjob_verifier(
            builder = 'try/fuchsia-compile-x64-dbg',
            experiment_percentage = 5,
        ),
        # https://crbug.com/739556; make this non-experimental ASAP.
        luci.cq_tryjob_verifier(
            builder = 'try/ios-device',
            experiment_percentage = 10,
        ),
        # https://crbug.com/739556
        luci.cq_tryjob_verifier(
            builder = 'try/ios-device-xcode-clang',
            experiment_percentage = 10,
        ),
        # https://crbug.com/739556
        luci.cq_tryjob_verifier(
            builder = 'try/ios-simulator-xcode-clang',
            experiment_percentage = 10,
        ),
    ],
)

luci.cq_group(
    name = 'cq-branches',
    cancel_stale_tryjobs = False,
    retry_config = cq.RETRY_ALL_FAILURES,
    tree_status_host = 'chromium-status.appspot.com/',
    watch = cq.refset(
        repo = 'https://chromium.googlesource.com/chromium/src',
        refs = ['refs/branch-heads/.+'],
    ),
    acls = [
        acl.entry(
            acl.CQ_COMMITTER,
            groups = 'project-chromium-committers',
        ),
        acl.entry(
            acl.CQ_DRY_RUNNER,
            groups = 'project-chromium-tryjob-access',
        ),
    ],
    verifiers = [
        luci.cq_tryjob_verifier(
            builder = builder,
            experiment_percentage = 100,
        ) for builder in [
            'linux-rel',
        ]
    ],
)
