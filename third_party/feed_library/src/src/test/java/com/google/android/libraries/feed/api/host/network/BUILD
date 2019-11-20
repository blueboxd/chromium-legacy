load("//src/main/java/com/google/android/libraries/feed/buildtools/testing:android_local_test.bzl", "DEFAULT_ANDROID_LOCAL_TEST_MANIFEST")

licenses(["notice"])  # Apache 2

android_local_test(
    name = "HttpRequestTest",
    size = "small",
    timeout = "moderate",
    srcs = ["HttpRequestTest.java"],
    manifest_values = DEFAULT_ANDROID_LOCAL_TEST_MANIFEST,
    deps = [
        "//src/main/java/com/google/android/libraries/feed/api/host/network",
        "//src/main/java/com/google/android/libraries/feed/common/testing",
        "//third_party:robolectric",
        "@com_google_protobuf_javalite//:protobuf_java_lite",
        "@maven//:com_google_truth_truth",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "HttpHeaderTest",
    size = "small",
    timeout = "moderate",
    srcs = ["HttpHeaderTest.java"],
    manifest_values = DEFAULT_ANDROID_LOCAL_TEST_MANIFEST,
    deps = [
        "//src/main/java/com/google/android/libraries/feed/api/host/network",
        "//third_party:robolectric",
        "@com_google_protobuf_javalite//:protobuf_java_lite",
        "@maven//:com_google_truth_truth",
        "@robolectric//bazel:android-all",
    ],
)
