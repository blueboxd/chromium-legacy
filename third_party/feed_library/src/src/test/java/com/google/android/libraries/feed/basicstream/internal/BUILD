load("//src/main/java/com/google/android/libraries/feed/buildtools/testing:android_local_test.bzl", "DEFAULT_ANDROID_LOCAL_TEST_MANIFEST")

licenses(["notice"])  # Apache 2

android_local_test(
    name = "StreamItemAnimatorTest",
    size = "small",
    timeout = "moderate",
    srcs = ["StreamItemAnimatorTest.java"],
    manifest_values = DEFAULT_ANDROID_LOCAL_TEST_MANIFEST,
    deps = [
        "//src/main/java/com/google/android/libraries/feed/api/client/stream",
        "//src/main/java/com/google/android/libraries/feed/basicstream/internal",
        "//src/main/java/com/google/android/libraries/feed/basicstream/internal/viewholders",
        "//third_party:robolectric",
        "@com_google_protobuf_javalite//:protobuf_java_lite",
        "@maven//:com_google_truth_truth",
        "@maven//:org_mockito_mockito_core",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "StreamItemTouchCallbacksTest",
    size = "small",
    timeout = "moderate",
    srcs = ["StreamItemTouchCallbacksTest.java"],
    manifest_values = DEFAULT_ANDROID_LOCAL_TEST_MANIFEST,
    deps = [
        "//src/main/java/com/google/android/libraries/feed/basicstream/internal",
        "//src/main/java/com/google/android/libraries/feed/basicstream/internal/viewholders",
        "//third_party:robolectric",
        "@com_google_protobuf_javalite//:protobuf_java_lite",
        "@maven//:com_android_support_recyclerview_v7",
        "@maven//:com_google_truth_truth",
        "@maven//:org_mockito_mockito_core",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "StreamRecyclerViewAdapterTest",
    size = "small",
    timeout = "moderate",
    srcs = ["StreamRecyclerViewAdapterTest.java"],
    manifest_values = DEFAULT_ANDROID_LOCAL_TEST_MANIFEST,
    deps = [
        "//src/main/java/com/google/android/libraries/feed/api/client/stream",
        "//src/main/java/com/google/android/libraries/feed/api/host/config",
        "//src/main/java/com/google/android/libraries/feed/api/host/stream",
        "//src/main/java/com/google/android/libraries/feed/basicstream/internal",
        "//src/main/java/com/google/android/libraries/feed/basicstream/internal/drivers",
        "//src/main/java/com/google/android/libraries/feed/basicstream/internal/viewholders",
        "//src/main/java/com/google/android/libraries/feed/common/functional",
        "//src/main/java/com/google/android/libraries/feed/piet",
        "//src/main/java/com/google/android/libraries/feed/piet/host",
        "//src/main/java/com/google/android/libraries/feed/sharedstream/deepestcontenttracker",
        "//src/main/java/com/google/android/libraries/feed/sharedstream/piet",
        "//src/main/java/com/google/android/libraries/feed/sharedstream/publicapi/scroll",
        "//src/main/java/com/google/android/libraries/feed/sharedstream/ui",
        "//third_party:robolectric",
        "@com_google_protobuf_javalite//:protobuf_java_lite",
        "@maven//:com_android_support_recyclerview_v7",
        "@maven//:com_google_guava_guava",
        "@maven//:com_google_truth_truth",
        "@maven//:org_mockito_mockito_core",
        "@robolectric//bazel:android-all",
    ],
)
