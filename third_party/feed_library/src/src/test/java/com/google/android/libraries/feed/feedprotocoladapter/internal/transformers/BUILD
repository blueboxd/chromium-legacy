load("//src/main/java/com/google/android/libraries/feed/buildtools/testing:android_local_test.bzl", "DEFAULT_ANDROID_LOCAL_TEST_MANIFEST")

licenses(["notice"])  # Apache 2

android_local_test(
    name = "ContentDataOperationTransformerTest",
    size = "small",
    timeout = "moderate",
    srcs = ["ContentDataOperationTransformerTest.java"],
    manifest_values = DEFAULT_ANDROID_LOCAL_TEST_MANIFEST,
    deps = [
        "//src/main/java/com/google/android/libraries/feed/feedprotocoladapter/internal/transformers",
        "//src/main/proto/com/google/android/libraries/feed/api/internal/proto:client_feed_java_proto_lite",
        "//src/main/proto/search/now/ui/stream:stream_java_proto_lite",
        "//src/main/proto/search/now/wire/feed:feed_java_proto_lite",
        "//third_party:robolectric",
        "@com_google_protobuf_javalite//:protobuf_java_lite",
        "@maven//:com_google_truth_truth",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "FeatureDataOperationTransformerTest",
    size = "small",
    timeout = "moderate",
    srcs = ["FeatureDataOperationTransformerTest.java"],
    manifest_values = DEFAULT_ANDROID_LOCAL_TEST_MANIFEST,
    deps = [
        "//src/main/java/com/google/android/libraries/feed/feedprotocoladapter/internal/transformers",
        "//src/main/proto/com/google/android/libraries/feed/api/internal/proto:client_feed_java_proto_lite",
        "//src/main/proto/search/now/ui/stream:stream_java_proto_lite",
        "//src/main/proto/search/now/wire/feed:feed_java_proto_lite",
        "//third_party:robolectric",
        "@com_google_protobuf_javalite//:protobuf_java_lite",
        "@maven//:com_google_truth_truth",
        "@robolectric//bazel:android-all",
    ],
)
