load("//src/main/java/com/google/android/libraries/feed/buildtools/testing:android_local_test.bzl", "DEFAULT_ANDROID_LOCAL_TEST_MANIFEST")

licenses(["notice"])  # Apache 2

android_local_test(
    name = "MenuMeasurerTest",
    size = "small",
    timeout = "moderate",
    srcs = ["MenuMeasurerTest.java"],
    manifest_values = DEFAULT_ANDROID_LOCAL_TEST_MANIFEST,
    deps = [
        "//src/main/java/com/google/android/libraries/feed/sharedstream/publicapi/menumeasurer",
        "//third_party:robolectric",
        "@com_google_protobuf_javalite//:protobuf_java_lite",
        "@maven//:com_google_guava_guava",
        "@maven//:com_google_truth_truth",
        "@robolectric//bazel:android-all",
    ],
)
