load("//src/main/java/com/google/android/libraries/feed/buildtools/testing:android_local_test.bzl", "DEFAULT_ANDROID_LOCAL_TEST_MANIFEST")

licenses(["notice"])  # Apache 2

android_local_test(
    name = "LocalActionGcTest",
    size = "small",
    timeout = "moderate",
    srcs = ["LocalActionGcTest.java"],
    manifest_values = DEFAULT_ANDROID_LOCAL_TEST_MANIFEST,
    deps = [
        "//src/main/java/com/google/android/libraries/feed/api/host/storage",
        "//src/main/java/com/google/android/libraries/feed/api/internal/store",
        "//src/main/java/com/google/android/libraries/feed/common/time",
        "//src/main/java/com/google/android/libraries/feed/feedstore/internal",
        "//src/main/proto/com/google/android/libraries/feed/api/internal/proto:client_feed_java_proto_lite",
        "//third_party:robolectric",
        "@com_google_protobuf_javalite//:protobuf_java_lite",
        "@maven//:com_google_truth_truth",
        "@maven//:org_mockito_mockito_core",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "ContentGcTest",
    size = "small",
    timeout = "moderate",
    srcs = ["ContentGcTest.java"],
    manifest_values = DEFAULT_ANDROID_LOCAL_TEST_MANIFEST,
    deps = [
        "//src/main/java/com/google/android/libraries/feed/api/host/config",
        "//src/main/java/com/google/android/libraries/feed/api/host/logging",
        "//src/main/java/com/google/android/libraries/feed/api/host/storage",
        "//src/main/java/com/google/android/libraries/feed/api/internal/store",
        "//src/main/java/com/google/android/libraries/feed/common",
        "//src/main/java/com/google/android/libraries/feed/common/concurrent",
        "//src/main/java/com/google/android/libraries/feed/common/concurrent/testing",
        "//src/main/java/com/google/android/libraries/feed/common/time",
        "//src/main/java/com/google/android/libraries/feed/common/time/testing",
        "//src/main/java/com/google/android/libraries/feed/feedstore/internal",
        "//src/main/proto/com/google/android/libraries/feed/api/internal/proto:client_feed_java_proto_lite",
        "//third_party:robolectric",
        "@com_google_protobuf_javalite//:protobuf_java_lite",
        "@maven//:com_google_guava_guava",
        "@maven//:com_google_truth_truth",
        "@maven//:org_mockito_mockito_core",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "EphemeralFeedStoreTest",
    size = "small",
    timeout = "moderate",
    srcs = ["EphemeralFeedStoreTest.java"],
    manifest_values = DEFAULT_ANDROID_LOCAL_TEST_MANIFEST,
    deps = [
        "//src/main/java/com/google/android/libraries/feed/api/internal/store",
        "//src/main/java/com/google/android/libraries/feed/common/concurrent",
        "//src/main/java/com/google/android/libraries/feed/feedstore/internal",
        "//src/main/java/com/google/android/libraries/feed/feedstore/testing",
        "//third_party:robolectric",
        "@com_google_protobuf_javalite//:protobuf_java_lite",
        "@maven//:org_mockito_mockito_core",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "PersistentFeedStoreTest",
    size = "small",
    timeout = "moderate",
    srcs = ["PersistentFeedStoreTest.java"],
    manifest_values = DEFAULT_ANDROID_LOCAL_TEST_MANIFEST,
    deps = [
        "//src/main/java/com/google/android/libraries/feed/api/host/config",
        "//src/main/java/com/google/android/libraries/feed/api/host/logging",
        "//src/main/java/com/google/android/libraries/feed/api/host/storage",
        "//src/main/java/com/google/android/libraries/feed/api/internal/common",
        "//src/main/java/com/google/android/libraries/feed/api/internal/store",
        "//src/main/java/com/google/android/libraries/feed/common",
        "//src/main/java/com/google/android/libraries/feed/common/concurrent",
        "//src/main/java/com/google/android/libraries/feed/common/concurrent/testing",
        "//src/main/java/com/google/android/libraries/feed/common/protoextensions",
        "//src/main/java/com/google/android/libraries/feed/feedstore/internal",
        "//src/main/java/com/google/android/libraries/feed/feedstore/testing",
        "//src/main/java/com/google/android/libraries/feed/hostimpl/storage/testing",
        "//src/main/java/com/google/android/libraries/feed/testing/host/logging",
        "//src/main/proto/com/google/android/libraries/feed/api/internal/proto:client_feed_java_proto_lite",
        "//third_party:robolectric",
        "@com_google_protobuf_javalite//:protobuf_java_lite",
        "@maven//:com_google_guava_guava",
        "@maven//:com_google_truth_truth",
        "@maven//:org_mockito_mockito_core",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "StreamPayloadInternerTest",
    size = "small",
    timeout = "moderate",
    srcs = ["StreamPayloadInternerTest.java"],
    manifest_values = DEFAULT_ANDROID_LOCAL_TEST_MANIFEST,
    deps = [
        "//src/main/java/com/google/android/libraries/feed/common/intern",
        "//src/main/java/com/google/android/libraries/feed/feedstore/internal",
        "//src/main/proto/com/google/android/libraries/feed/api/internal/proto:client_feed_java_proto_lite",
        "//third_party:robolectric",
        "@com_google_protobuf_javalite//:protobuf_java_lite",
        "@maven//:com_google_truth_truth",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "StreamStructureInternerTest",
    size = "small",
    timeout = "moderate",
    srcs = ["StreamStructureInternerTest.java"],
    manifest_values = DEFAULT_ANDROID_LOCAL_TEST_MANIFEST,
    deps = [
        "//src/main/java/com/google/android/libraries/feed/common/intern",
        "//src/main/java/com/google/android/libraries/feed/feedstore/internal",
        "//src/main/proto/com/google/android/libraries/feed/api/internal/proto:client_feed_java_proto_lite",
        "//third_party:robolectric",
        "@com_google_protobuf_javalite//:protobuf_java_lite",
        "@maven//:com_google_truth_truth",
        "@robolectric//bazel:android-all",
    ],
)
