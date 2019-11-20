load("//src/main/java/com/google/android/libraries/feed/buildtools/testing:android_local_test.bzl", "DEFAULT_ANDROID_LOCAL_TEST_MANIFEST")

licenses(["notice"])  # Apache 2

android_local_test(
    name = "FeatureChangeImplTest",
    size = "small",
    timeout = "moderate",
    srcs = ["FeatureChangeImplTest.java"],
    manifest_values = DEFAULT_ANDROID_LOCAL_TEST_MANIFEST,
    deps = [
        "//src/main/java/com/google/android/libraries/feed/api/internal/common/testing",
        "//src/main/java/com/google/android/libraries/feed/api/internal/modelprovider",
        "//src/main/java/com/google/android/libraries/feed/feedmodelprovider/internal",
        "//src/main/proto/com/google/android/libraries/feed/api/internal/proto:client_feed_java_proto_lite",
        "//third_party:robolectric",
        "@com_google_protobuf_javalite//:protobuf_java_lite",
        "@maven//:com_google_truth_truth",
        "@maven//:org_mockito_mockito_core",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "ModelCursorImplTest",
    size = "small",
    timeout = "moderate",
    srcs = ["ModelCursorImplTest.java"],
    manifest_values = DEFAULT_ANDROID_LOCAL_TEST_MANIFEST,
    deps = [
        "//src/main/java/com/google/android/libraries/feed/api/internal/common/testing",
        "//src/main/java/com/google/android/libraries/feed/api/internal/modelprovider",
        "//src/main/java/com/google/android/libraries/feed/feedmodelprovider/internal",
        "//third_party:robolectric",
        "@com_google_protobuf_javalite//:protobuf_java_lite",
        "@maven//:com_google_truth_truth",
        "@maven//:org_mockito_mockito_core",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "ModelFeatureImplTest",
    size = "small",
    timeout = "moderate",
    srcs = ["ModelFeatureImplTest.java"],
    manifest_values = DEFAULT_ANDROID_LOCAL_TEST_MANIFEST,
    deps = [
        "//src/main/java/com/google/android/libraries/feed/api/internal/modelprovider",
        "//src/main/java/com/google/android/libraries/feed/feedmodelprovider/internal",
        "//src/main/proto/com/google/android/libraries/feed/api/internal/proto:client_feed_java_proto_lite",
        "//third_party:robolectric",
        "@com_google_protobuf_javalite//:protobuf_java_lite",
        "@maven//:com_google_truth_truth",
        "@maven//:org_mockito_mockito_core",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "ModelMutationImplTest",
    size = "small",
    timeout = "moderate",
    srcs = ["ModelMutationImplTest.java"],
    manifest_values = DEFAULT_ANDROID_LOCAL_TEST_MANIFEST,
    deps = [
        "//src/main/java/com/google/android/libraries/feed/api/common",
        "//src/main/java/com/google/android/libraries/feed/common/functional",
        "//src/main/java/com/google/android/libraries/feed/feedmodelprovider/internal",
        "//src/main/proto/com/google/android/libraries/feed/api/internal/proto:client_feed_java_proto_lite",
        "//third_party:robolectric",
        "@com_google_protobuf_javalite//:protobuf_java_lite",
        "@maven//:com_google_truth_truth",
        "@maven//:org_mockito_mockito_core",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "ModelTokenImplTest",
    size = "small",
    timeout = "moderate",
    srcs = ["ModelTokenImplTest.java"],
    manifest_values = DEFAULT_ANDROID_LOCAL_TEST_MANIFEST,
    deps = [
        "//src/main/java/com/google/android/libraries/feed/api/internal/modelprovider",
        "//src/main/java/com/google/android/libraries/feed/feedmodelprovider/internal",
        "//src/main/proto/com/google/android/libraries/feed/api/internal/proto:client_feed_java_proto_lite",
        "//third_party:robolectric",
        "@com_google_protobuf_javalite//:protobuf_java_lite",
        "@maven//:com_google_truth_truth",
        "@maven//:org_mockito_mockito_core",
        "@robolectric//bazel:android-all",
    ],
)
