# List of CQ builders

This page is auto generated using the script
//infra/config/cq_cfg_presubmit.py. Do not manually edit.

[TOC]

Each builder name links to that builder on Milo. The "Backing builders" links
point to the file used to determine which configurations a builder should copy
when running. These links might 404 or error; they are hard-coded right now,
using common assumptions about how builders are configured.

## Required builders

These builders must pass before a CL may land.

* [android-binary-size](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/android-binary-size) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/android-binary-size)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+android-binary-size))

* [android-kitkat-arm-rel](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/android-kitkat-arm-rel) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/android-kitkat-arm-rel)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+android-kitkat-arm-rel))

* [android-marshmallow-arm64-rel](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/android-marshmallow-arm64-rel) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/android-marshmallow-arm64-rel)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+android-marshmallow-arm64-rel))

* [android_arm64_dbg_recipe](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/android_arm64_dbg_recipe) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/android_arm64_dbg_recipe)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+android_arm64_dbg_recipe))

* [android_clang_dbg_recipe](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/android_clang_dbg_recipe) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/android_clang_dbg_recipe)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+android_clang_dbg_recipe))

* [android_compile_dbg](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/android_compile_dbg) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/android_compile_dbg)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+android_compile_dbg))

* [android_cronet](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/android_cronet) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/android_cronet)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+android_cronet))

* [cast_shell_android](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/cast_shell_android) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/cast_shell_android)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+cast_shell_android))

* [cast_shell_linux](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/cast_shell_linux) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/cast_shell_linux)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+cast_shell_linux))

* [chromeos-amd64-generic-rel](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/chromeos-amd64-generic-rel) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/chromeos-amd64-generic-rel)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+chromeos-amd64-generic-rel))

* [chromeos-arm-generic-rel](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/chromeos-arm-generic-rel) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/chromeos-arm-generic-rel)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+chromeos-arm-generic-rel))

* [chromium_presubmit](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/chromium_presubmit) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/chromium_presubmit)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+chromium_presubmit))

* [fuchsia_arm64](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/fuchsia_arm64) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/fuchsia_arm64)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+fuchsia_arm64))

* [fuchsia_x64](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/fuchsia_x64) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/fuchsia_x64)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+fuchsia_x64))

* [ios-simulator](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/ios-simulator) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/ios-simulator)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+ios-simulator))

* [linux-chromeos-compile-dbg](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/linux-chromeos-compile-dbg) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/linux-chromeos-compile-dbg)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+linux-chromeos-compile-dbg))

* [linux-chromeos-rel](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/linux-chromeos-rel) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/linux-chromeos-rel)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+linux-chromeos-rel))

* [linux-jumbo-rel](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/linux-jumbo-rel) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/linux-jumbo-rel)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+linux-jumbo-rel))

* [linux-libfuzzer-asan-rel](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/linux-libfuzzer-asan-rel) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/linux-libfuzzer-asan-rel)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+linux-libfuzzer-asan-rel))

* [linux-ozone-rel](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/linux-ozone-rel) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/linux-ozone-rel)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+linux-ozone-rel))

* [linux-rel](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/linux-rel) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/linux-rel)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+linux-rel))

* [linux_chromium_asan_rel_ng](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/linux_chromium_asan_rel_ng) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/linux_chromium_asan_rel_ng)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+linux_chromium_asan_rel_ng))

* [linux_chromium_compile_dbg_ng](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/linux_chromium_compile_dbg_ng) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/linux_chromium_compile_dbg_ng)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+linux_chromium_compile_dbg_ng))

* [linux_chromium_tsan_rel_ng](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/linux_chromium_tsan_rel_ng) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/linux_chromium_tsan_rel_ng)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+linux_chromium_tsan_rel_ng))

* [mac-rel](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/mac-rel) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/mac-rel)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+mac-rel))

* [mac_chromium_compile_dbg_ng](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/mac_chromium_compile_dbg_ng) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/mac_chromium_compile_dbg_ng)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+mac_chromium_compile_dbg_ng))

* [win-libfuzzer-asan-rel](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/win-libfuzzer-asan-rel) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/win-libfuzzer-asan-rel)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+win-libfuzzer-asan-rel))

* [win10_chromium_x64_rel_ng](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/win10_chromium_x64_rel_ng) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/win10_chromium_x64_rel_ng)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+win10_chromium_x64_rel_ng))

* [win7-rel](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/win7-rel) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/win7-rel)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+win7-rel))

* [win_chromium_compile_dbg_ng](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/win_chromium_compile_dbg_ng) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/win_chromium_compile_dbg_ng)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+win_chromium_compile_dbg_ng))


## Optional builders

These builders optionally run, depending on the files in a
CL. For example, a CL which touches `//gpu/BUILD.gn` would trigger the builder
`android_optional_gpu_tests_rel`, due to the `location_regexp` values for that
builder.

* [android-cronet-arm-dbg](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/android-cronet-arm-dbg) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/android-cronet-arm-dbg)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+android-cronet-arm-dbg))

  Path regular expressions:
    * [`//components/cronet/.+`](https://cs.chromium.org/chromium/src/components/cronet/)
    * [`//components/grpc_support/.+`](https://cs.chromium.org/chromium/src/components/grpc_support/)
    * [`//build/android/.+`](https://cs.chromium.org/chromium/src/build/android/)
    * [`//build/config/android/.+`](https://cs.chromium.org/chromium/src/build/config/android/)

* [android_compile_x64_dbg](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/android_compile_x64_dbg) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/android_compile_x64_dbg)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+android_compile_x64_dbg))

  Path regular expressions:
    * [`//sandbox/linux/seccomp-bpf/.+`](https://cs.chromium.org/chromium/src/sandbox/linux/seccomp-bpf/)
    * [`//sandbox/linux/seccomp-bpf-helpers/.+`](https://cs.chromium.org/chromium/src/sandbox/linux/seccomp-bpf-helpers/)
    * [`//sandbox/linux/system_headers/.+`](https://cs.chromium.org/chromium/src/sandbox/linux/system_headers/)
    * [`//sandbox/linux/tests/.+`](https://cs.chromium.org/chromium/src/sandbox/linux/tests/)

* [android_compile_x86_dbg](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/android_compile_x86_dbg) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/android_compile_x86_dbg)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+android_compile_x86_dbg))

  Path regular expressions:
    * [`//sandbox/linux/seccomp-bpf/.+`](https://cs.chromium.org/chromium/src/sandbox/linux/seccomp-bpf/)
    * [`//sandbox/linux/seccomp-bpf-helpers/.+`](https://cs.chromium.org/chromium/src/sandbox/linux/seccomp-bpf-helpers/)
    * [`//sandbox/linux/system_headers/.+`](https://cs.chromium.org/chromium/src/sandbox/linux/system_headers/)
    * [`//sandbox/linux/tests/.+`](https://cs.chromium.org/chromium/src/sandbox/linux/tests/)

* [android_optional_gpu_tests_rel](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/android_optional_gpu_tests_rel) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/android_optional_gpu_tests_rel)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+android_optional_gpu_tests_rel))

  Path regular expressions:
    * [`//cc/.+`](https://cs.chromium.org/chromium/src/cc/)
    * [`//chrome/browser/vr/.+`](https://cs.chromium.org/chromium/src/chrome/browser/vr/)
    * [`//components/viz/.+`](https://cs.chromium.org/chromium/src/components/viz/)
    * [`//content/test/gpu/.+`](https://cs.chromium.org/chromium/src/content/test/gpu/)
    * [`//gpu/.+`](https://cs.chromium.org/chromium/src/gpu/)
    * [`//media/audio/.+`](https://cs.chromium.org/chromium/src/media/audio/)
    * [`//media/filters/.+`](https://cs.chromium.org/chromium/src/media/filters/)
    * [`//media/gpu/.+`](https://cs.chromium.org/chromium/src/media/gpu/)
    * [`//services/viz/.+`](https://cs.chromium.org/chromium/src/services/viz/)
    * [`//testing/trigger_scripts/.+`](https://cs.chromium.org/chromium/src/testing/trigger_scripts/)
    * [`//third_party/blink/renderer/modules/webgl/.+`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/modules/webgl/)
    * [`//third_party/blink/renderer/platform/graphics/gpu/.+`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/platform/graphics/gpu/)
    * [`//ui/gl/.+`](https://cs.chromium.org/chromium/src/ui/gl/)

* [chromeos-kevin-compile-rel](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/chromeos-kevin-compile-rel) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/chromeos-kevin-compile-rel)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+chromeos-kevin-compile-rel))

  Path regular expressions:
    * [`//chromeos/CHROMEOS_LKGM`](https://cs.chromium.org/search/?q=package:%5Echromium$+file:chromeos/CHROMEOS_LKGM)

* [chromeos-kevin-rel](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/chromeos-kevin-rel) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/chromeos-kevin-rel)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+chromeos-kevin-rel))

  Path regular expressions:
    * [`//build/chromeos/.+`](https://cs.chromium.org/chromium/src/build/chromeos/)
    * [`//build/config/chromeos/.*`](https://cs.chromium.org/search/?q=package:%5Echromium$+file:build/config/chromeos/.*)

* [closure_compilation](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/closure_compilation) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/closure_compilation)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+closure_compilation))

  Path regular expressions:
    * [`//third_party/closure_compiler/.+`](https://cs.chromium.org/chromium/src/third_party/closure_compiler/)

* [dawn-linux-x64-deps-rel](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/dawn-linux-x64-deps-rel) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/dawn-linux-x64-deps-rel)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+dawn-linux-x64-deps-rel))

  Path regular expressions:
    * [`//testing/buildbot/chromium.dawn.json`](https://cs.chromium.org/search/?q=package:%5Echromium$+file:testing/buildbot/chromium.dawn.json)
    * [`//third_party/blink/renderer/modules/webgpu/.+`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/modules/webgpu/)
    * [`//third_party/blink/web_tests/FlagExpectations/enable-unsafe-webgpu`](https://cs.chromium.org/search/?q=package:%5Echromium$+file:third_party/blink/web_tests/FlagExpectations/enable-unsafe-webgpu)
    * [`//third_party/blink/web_tests/webgpu/.+`](https://cs.chromium.org/chromium/src/third_party/blink/web_tests/webgpu/)
    * [`//third_party/dawn/.+`](https://cs.chromium.org/chromium/src/third_party/dawn/)

* [dawn-mac-x64-deps-rel](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/dawn-mac-x64-deps-rel) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/dawn-mac-x64-deps-rel)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+dawn-mac-x64-deps-rel))

  Path regular expressions:
    * [`//gpu/.+`](https://cs.chromium.org/chromium/src/gpu/)
    * [`//testing/buildbot/chromium.dawn.json`](https://cs.chromium.org/search/?q=package:%5Echromium$+file:testing/buildbot/chromium.dawn.json)
    * [`//third_party/blink/renderer/modules/webgpu/.+`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/modules/webgpu/)
    * [`//third_party/blink/web_tests/FlagExpectations/enable-unsafe-webgpu`](https://cs.chromium.org/search/?q=package:%5Echromium$+file:third_party/blink/web_tests/FlagExpectations/enable-unsafe-webgpu)
    * [`//third_party/blink/web_tests/webgpu/.+`](https://cs.chromium.org/chromium/src/third_party/blink/web_tests/webgpu/)
    * [`//third_party/dawn/.+`](https://cs.chromium.org/chromium/src/third_party/dawn/)

* [dawn-win10-x64-deps-rel](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/dawn-win10-x64-deps-rel) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/dawn-win10-x64-deps-rel)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+dawn-win10-x64-deps-rel))

  Path regular expressions:
    * [`//gpu/.+`](https://cs.chromium.org/chromium/src/gpu/)
    * [`//testing/buildbot/chromium.dawn.json`](https://cs.chromium.org/search/?q=package:%5Echromium$+file:testing/buildbot/chromium.dawn.json)
    * [`//third_party/blink/renderer/modules/webgpu/.+`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/modules/webgpu/)
    * [`//third_party/blink/web_tests/FlagExpectations/enable-unsafe-webgpu`](https://cs.chromium.org/search/?q=package:%5Echromium$+file:third_party/blink/web_tests/FlagExpectations/enable-unsafe-webgpu)
    * [`//third_party/blink/web_tests/webgpu/.+`](https://cs.chromium.org/chromium/src/third_party/blink/web_tests/webgpu/)
    * [`//third_party/dawn/.+`](https://cs.chromium.org/chromium/src/third_party/dawn/)

* [dawn-win10-x86-deps-rel](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/dawn-win10-x86-deps-rel) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/dawn-win10-x86-deps-rel)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+dawn-win10-x86-deps-rel))

  Path regular expressions:
    * [`//gpu/.+`](https://cs.chromium.org/chromium/src/gpu/)
    * [`//testing/buildbot/chromium.dawn.json`](https://cs.chromium.org/search/?q=package:%5Echromium$+file:testing/buildbot/chromium.dawn.json)
    * [`//third_party/blink/renderer/modules/webgpu/.+`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/modules/webgpu/)
    * [`//third_party/blink/web_tests/FlagExpectations/enable-unsafe-webgpu`](https://cs.chromium.org/search/?q=package:%5Echromium$+file:third_party/blink/web_tests/FlagExpectations/enable-unsafe-webgpu)
    * [`//third_party/blink/web_tests/webgpu/.+`](https://cs.chromium.org/chromium/src/third_party/blink/web_tests/webgpu/)
    * [`//third_party/dawn/.+`](https://cs.chromium.org/chromium/src/third_party/dawn/)

* [fuchsia-arm64-cast](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/fuchsia-arm64-cast) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/fuchsia-arm64-cast)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+fuchsia-arm64-cast))

  Path regular expressions:
    * [`//chromecast/.+`](https://cs.chromium.org/chromium/src/chromecast/)

* [fuchsia-x64-cast](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/fuchsia-x64-cast) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/fuchsia-x64-cast)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+fuchsia-x64-cast))

  Path regular expressions:
    * [`//chromecast/.+`](https://cs.chromium.org/chromium/src/chromecast/)

* [ios-simulator-cronet](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/ios-simulator-cronet) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/ios-simulator-cronet)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+ios-simulator-cronet))

  Path regular expressions:
    * [`//components/cronet/.+`](https://cs.chromium.org/chromium/src/components/cronet/)
    * [`//components/grpc_support/.+`](https://cs.chromium.org/chromium/src/components/grpc_support/)
    * [`//ios/.+`](https://cs.chromium.org/chromium/src/ios/)

* [ios-simulator-full-configs](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/ios-simulator-full-configs) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/ios-simulator-full-configs)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+ios-simulator-full-configs))

  Path regular expressions:
    * [`//ios/.+`](https://cs.chromium.org/chromium/src/ios/)

* [linux-blink-rel](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/linux-blink-rel) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/linux-blink-rel)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+linux-blink-rel))

  Path regular expressions:
    * [`//cc/.+`](https://cs.chromium.org/chromium/src/cc/)
    * [`//third_party/blink/renderer/core/paint/.+`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/paint/)
    * [`//third_party/blink/renderer/core/svg/.+`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/svg/)
    * [`//third_party/blink/renderer/platform/graphics/.+`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/platform/graphics/)
    * [`//third_party/blink/web_tests/FlagExpectations/enable-blink-features=CompositeAfterPaint`](https://cs.chromium.org/search/?q=package:%5Echromium$+file:third_party/blink/web_tests/FlagExpectations/enable-blink-features=CompositeAfterPaint)
    * [`//third_party/blink/web_tests/flag-specific/enable-blink-features=CompositeAfterPaint/.+`](https://cs.chromium.org/chromium/src/third_party/blink/web_tests/flag-specific/enable-blink-features=CompositeAfterPaint/)

* [linux_chromium_dbg_ng](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/linux_chromium_dbg_ng) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/linux_chromium_dbg_ng)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+linux_chromium_dbg_ng))

  Path regular expressions:
    * [`//build/.*check_gn_headers.*`](https://cs.chromium.org/search/?q=package:%5Echromium$+file:build/.*check_gn_headers.*)

* [linux_layout_tests_composite_after_paint](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/linux_layout_tests_composite_after_paint) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/linux_layout_tests_composite_after_paint)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+linux_layout_tests_composite_after_paint))

  Path regular expressions:
    * [`//third_party/blink/renderer/core/paint/.+`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/paint/)
    * [`//third_party/blink/renderer/core/svg/.+`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/svg/)
    * [`//third_party/blink/renderer/platform/graphics/.+`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/platform/graphics/)
    * [`//third_party/blink/web_tests/FlagExpectations/enable-blink-features=CompositeAfterPaint`](https://cs.chromium.org/search/?q=package:%5Echromium$+file:third_party/blink/web_tests/FlagExpectations/enable-blink-features=CompositeAfterPaint)
    * [`//third_party/blink/web_tests/flag-specific/enable-blink-features=CompositeAfterPaint/.+`](https://cs.chromium.org/chromium/src/third_party/blink/web_tests/flag-specific/enable-blink-features=CompositeAfterPaint/)

* [linux_layout_tests_layout_ng](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/linux_layout_tests_layout_ng) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/linux_layout_tests_layout_ng)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+linux_layout_tests_layout_ng))

  Path regular expressions:
    * [`//third_party/blink/renderer/core/editing/.+`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/editing/)
    * [`//third_party/blink/renderer/core/layout/.+`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/layout/)
    * [`//third_party/blink/renderer/core/paint/.+`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/paint/)
    * [`//third_party/blink/renderer/core/svg/.+`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/svg/)
    * [`//third_party/blink/renderer/platform/fonts/shaping/.+`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/platform/fonts/shaping/)
    * [`//third_party/blink/renderer/platform/graphics/.+`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/platform/graphics/)
    * [`//third_party/blink/web_tests/FlagExpectations/enable-blink-features=LayoutNG`](https://cs.chromium.org/search/?q=package:%5Echromium$+file:third_party/blink/web_tests/FlagExpectations/enable-blink-features=LayoutNG)
    * [`//third_party/blink/web_tests/flag-specific/enable-blink-features=LayoutNG/.+`](https://cs.chromium.org/chromium/src/third_party/blink/web_tests/flag-specific/enable-blink-features=LayoutNG/)

* [linux_optional_gpu_tests_rel](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/linux_optional_gpu_tests_rel) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/linux_optional_gpu_tests_rel)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+linux_optional_gpu_tests_rel))

  Path regular expressions:
    * [`//chrome/browser/vr/.+`](https://cs.chromium.org/chromium/src/chrome/browser/vr/)
    * [`//content/test/gpu/.+`](https://cs.chromium.org/chromium/src/content/test/gpu/)
    * [`//gpu/.+`](https://cs.chromium.org/chromium/src/gpu/)
    * [`//media/audio/.+`](https://cs.chromium.org/chromium/src/media/audio/)
    * [`//media/filters/.+`](https://cs.chromium.org/chromium/src/media/filters/)
    * [`//media/gpu/.+`](https://cs.chromium.org/chromium/src/media/gpu/)
    * [`//testing/buildbot/chromium.gpu.fyi.json`](https://cs.chromium.org/search/?q=package:%5Echromium$+file:testing/buildbot/chromium.gpu.fyi.json)
    * [`//testing/trigger_scripts/.+`](https://cs.chromium.org/chromium/src/testing/trigger_scripts/)
    * [`//third_party/blink/renderer/modules/webgl/.+`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/modules/webgl/)
    * [`//third_party/blink/renderer/platform/graphics/gpu/.+`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/platform/graphics/gpu/)
    * [`//ui/gl/.+`](https://cs.chromium.org/chromium/src/ui/gl/)

* [linux_vr](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/linux_vr) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/linux_vr)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+linux_vr))

  Path regular expressions:
    * [`//chrome/browser/vr/.+`](https://cs.chromium.org/chromium/src/chrome/browser/vr/)

* [mac_optional_gpu_tests_rel](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/mac_optional_gpu_tests_rel) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/mac_optional_gpu_tests_rel)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+mac_optional_gpu_tests_rel))

  Path regular expressions:
    * [`//chrome/browser/vr/.+`](https://cs.chromium.org/chromium/src/chrome/browser/vr/)
    * [`//content/test/gpu/.+`](https://cs.chromium.org/chromium/src/content/test/gpu/)
    * [`//gpu/.+`](https://cs.chromium.org/chromium/src/gpu/)
    * [`//media/audio/.+`](https://cs.chromium.org/chromium/src/media/audio/)
    * [`//media/filters/.+`](https://cs.chromium.org/chromium/src/media/filters/)
    * [`//media/gpu/.+`](https://cs.chromium.org/chromium/src/media/gpu/)
    * [`//services/shape_detection/.+`](https://cs.chromium.org/chromium/src/services/shape_detection/)
    * [`//testing/buildbot/chromium.gpu.fyi.json`](https://cs.chromium.org/search/?q=package:%5Echromium$+file:testing/buildbot/chromium.gpu.fyi.json)
    * [`//testing/trigger_scripts/.+`](https://cs.chromium.org/chromium/src/testing/trigger_scripts/)
    * [`//third_party/blink/renderer/modules/webgl/.+`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/modules/webgl/)
    * [`//third_party/blink/renderer/platform/graphics/gpu/.+`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/platform/graphics/gpu/)
    * [`//ui/gl/.+`](https://cs.chromium.org/chromium/src/ui/gl/)

* [win_optional_gpu_tests_rel](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/win_optional_gpu_tests_rel) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/win_optional_gpu_tests_rel)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+win_optional_gpu_tests_rel))

  Path regular expressions:
    * [`//chrome/browser/vr/.+`](https://cs.chromium.org/chromium/src/chrome/browser/vr/)
    * [`//content/test/gpu/.+`](https://cs.chromium.org/chromium/src/content/test/gpu/)
    * [`//device/vr/.+`](https://cs.chromium.org/chromium/src/device/vr/)
    * [`//gpu/.+`](https://cs.chromium.org/chromium/src/gpu/)
    * [`//media/audio/.+`](https://cs.chromium.org/chromium/src/media/audio/)
    * [`//media/filters/.+`](https://cs.chromium.org/chromium/src/media/filters/)
    * [`//media/gpu/.+`](https://cs.chromium.org/chromium/src/media/gpu/)
    * [`//testing/buildbot/chromium.gpu.fyi.json`](https://cs.chromium.org/search/?q=package:%5Echromium$+file:testing/buildbot/chromium.gpu.fyi.json)
    * [`//testing/trigger_scripts/.+`](https://cs.chromium.org/chromium/src/testing/trigger_scripts/)
    * [`//third_party/blink/renderer/modules/vr/.+`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/modules/vr/)
    * [`//third_party/blink/renderer/modules/webgl/.+`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/modules/webgl/)
    * [`//third_party/blink/renderer/modules/xr/.+`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/modules/xr/)
    * [`//third_party/blink/renderer/platform/graphics/gpu/.+`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/platform/graphics/gpu/)
    * [`//ui/gl/.+`](https://cs.chromium.org/chromium/src/ui/gl/)


## Experimental builders

These builders are run on some percentage of builds. Their results are ignored
by CQ. These are often used to test new configurations before they are added
as required builders.

* [ios-device](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/ios-device) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/ios-device)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+ios-device))

  https://crbug.com/739556; make this non-experimental ASAP.

  * Experimental percentage: 10

* [ios-device-xcode-clang](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/ios-device-xcode-clang) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/ios-device-xcode-clang)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+ios-device-xcode-clang))

  https://crbug.com/739556

  * Experimental percentage: 10

* [ios-simulator-xcode-clang](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/ios-simulator-xcode-clang) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/ios-simulator-xcode-clang)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+ios-simulator-xcode-clang))

  https://crbug.com/739556

  * Experimental percentage: 10

* [linux-coverage-rel](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/linux-coverage-rel) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/linux-coverage-rel)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+linux-coverage-rel))

  * Experimental percentage: 10

* [win7_chromium_rel_loc_exp](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/win7_chromium_rel_loc_exp) ([`commit-queue.cfg` entry](https://cs.chromium.org/search/?q=package:%5Echromium$+file:commit-queue.cfg+chromium/try/win7_chromium_rel_loc_exp)) ([matching builders](https://cs.chromium.org/search/?q=file:trybots.py+win7_chromium_rel_loc_exp))

  * Experimental percentage: 20

