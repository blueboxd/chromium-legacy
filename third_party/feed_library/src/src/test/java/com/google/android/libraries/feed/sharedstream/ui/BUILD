load("//src/main/java/com/google/android/libraries/feed/buildtools/testing:android_local_test.bzl", "DEFAULT_ANDROID_LOCAL_TEST_MANIFEST")

licenses(["notice"])  # Apache 2

android_local_test(
    name = "MaterialSpinnerViewTest",
    size = "small",
    timeout = "moderate",
    srcs = ["MaterialSpinnerViewTest.java"],
    manifest_values = DEFAULT_ANDROID_LOCAL_TEST_MANIFEST,
    deps = [
        "//src/main/java/com/google/android/libraries/feed/sharedstream/ui",
        "//third_party:robolectric",
        "@com_google_protobuf_javalite//:protobuf_java_lite",
        "@maven//:com_android_support_swiperefreshlayout",
        "@maven//:com_google_truth_truth",
        "@robolectric//bazel:android-all",
    ],
)
