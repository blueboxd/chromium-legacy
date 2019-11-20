load("//src/main/java/com/google/android/libraries/feed/buildtools/testing:android_local_test.bzl", "DEFAULT_ANDROID_LOCAL_TEST_MANIFEST")

licenses(["notice"])  # Apache 2

android_local_test(
    name = "FeedActionParserTest",
    size = "small",
    timeout = "moderate",
    srcs = ["FeedActionParserTest.java"],
    manifest_values = DEFAULT_ANDROID_LOCAL_TEST_MANIFEST,
    deps = [
        "//src/main/java/com/google/android/libraries/feed/api/client/knowncontent",
        "//src/main/java/com/google/android/libraries/feed/api/host/action",
        "//src/main/java/com/google/android/libraries/feed/api/host/logging",
        "//src/main/java/com/google/android/libraries/feed/api/host/stream",
        "//src/main/java/com/google/android/libraries/feed/api/internal/actionparser",
        "//src/main/java/com/google/android/libraries/feed/api/internal/protocoladapter",
        "//src/main/java/com/google/android/libraries/feed/common",
        "//src/main/java/com/google/android/libraries/feed/feedactionparser",
        "//src/main/java/com/google/android/libraries/feed/feedactionparser/internal",
        "//src/main/proto/com/google/android/libraries/feed/api/internal/proto:client_feed_java_proto_lite",
        "//src/main/proto/search/now/ui/action:feed_action_java_proto_lite",
        "//src/main/proto/search/now/ui/action:feed_action_payload_java_proto_lite",
        "//src/main/proto/search/now/ui/action:piet_extensions_java_proto_lite",
        "//src/main/proto/search/now/ui/piet:piet_java_proto_lite",
        "//src/main/proto/search/now/wire/feed:feed_java_proto_lite",
        "//third_party:robolectric",
        "@com_google_protobuf_javalite//:protobuf_java_lite",
        "@maven//:com_google_guava_guava",
        "@maven//:com_google_truth_truth",
        "@maven//:org_mockito_mockito_core",
        "@robolectric//bazel:android-all",
    ],
)
