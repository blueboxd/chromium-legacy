load("//src/main/java/com/google/android/libraries/feed/buildtools/testing:android_local_test.bzl", "DEFAULT_ANDROID_LOCAL_TEST_MANIFEST")

licenses(["notice"])  # Apache 2

android_local_test(
    name = "OneShotVisibilityLoggingListenerTest",
    size = "small",
    timeout = "moderate",
    srcs = ["OneShotVisibilityLoggingListenerTest.java"],
    manifest_values = DEFAULT_ANDROID_LOCAL_TEST_MANIFEST,
    deps = [
        "//src/main/java/com/google/android/libraries/feed/sharedstream/logging",
        "//third_party:robolectric",
        "@com_google_protobuf_javalite//:protobuf_java_lite",
        "@maven//:org_mockito_mockito_core",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "SpinnerLoggerTest",
    size = "small",
    timeout = "moderate",
    srcs = ["SpinnerLoggerTest.java"],
    manifest_values = DEFAULT_ANDROID_LOCAL_TEST_MANIFEST,
    deps = [
        "//src/main/java/com/google/android/libraries/feed/api/host/logging",
        "//src/main/java/com/google/android/libraries/feed/common/testing",
        "//src/main/java/com/google/android/libraries/feed/common/time/testing",
        "//src/main/java/com/google/android/libraries/feed/sharedstream/logging",
        "//third_party:robolectric",
        "@com_google_protobuf_javalite//:protobuf_java_lite",
        "@maven//:com_google_truth_truth",
        "@maven//:org_mockito_mockito_core",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "UiSessionRequestLoggerTest",
    size = "small",
    timeout = "moderate",
    srcs = ["UiSessionRequestLoggerTest.java"],
    manifest_values = DEFAULT_ANDROID_LOCAL_TEST_MANIFEST,
    deps = [
        "//src/main/java/com/google/android/libraries/feed/api/host/logging",
        "//src/main/java/com/google/android/libraries/feed/api/internal/modelprovider",
        "//src/main/java/com/google/android/libraries/feed/common/time/testing",
        "//src/main/java/com/google/android/libraries/feed/sharedstream/logging",
        "//src/main/java/com/google/android/libraries/feed/testing/modelprovider",
        "//third_party:robolectric",
        "@com_google_protobuf_javalite//:protobuf_java_lite",
        "@maven//:com_google_truth_truth",
        "@maven//:org_mockito_mockito_core",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "VisibilityMonitorTest",
    size = "small",
    timeout = "moderate",
    srcs = ["VisibilityMonitorTest.java"],
    manifest_values = DEFAULT_ANDROID_LOCAL_TEST_MANIFEST,
    deps = [
        "//src/main/java/com/google/android/libraries/feed/api/host/config",
        "//src/main/java/com/google/android/libraries/feed/sharedstream/logging",
        "//third_party:robolectric",
        "@com_google_protobuf_javalite//:protobuf_java_lite",
        "@maven//:org_mockito_mockito_core",
        "@robolectric//bazel:android-all",
    ],
)
