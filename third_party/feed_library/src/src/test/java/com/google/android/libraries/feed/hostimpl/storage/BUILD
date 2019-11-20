load("//src/main/java/com/google/android/libraries/feed/buildtools/testing:android_local_test.bzl", "DEFAULT_ANDROID_LOCAL_TEST_MANIFEST")

licenses(["notice"])  # Apache 2

android_local_test(
    name = "PersistentContentStorageTest",
    size = "small",
    timeout = "moderate",
    srcs = ["PersistentContentStorageTest.java"],
    manifest_values = DEFAULT_ANDROID_LOCAL_TEST_MANIFEST,
    deps = [
        "//src/main/java/com/google/android/libraries/feed/api/internal/common",
        "//src/main/java/com/google/android/libraries/feed/hostimpl/storage",
        "//src/main/java/com/google/android/libraries/feed/testing/conformance/storage",
        "//third_party:robolectric",
        "@com_google_protobuf_javalite//:protobuf_java_lite",
        "@maven//:androidx_test_core",
        "@maven//:com_google_guava_guava",
        "@maven//:org_mockito_mockito_core",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "PersistentContentStorageDirectTest",
    size = "small",
    timeout = "moderate",
    srcs = ["PersistentContentStorageDirectTest.java"],
    manifest_values = DEFAULT_ANDROID_LOCAL_TEST_MANIFEST,
    deps = [
        "//src/main/java/com/google/android/libraries/feed/api/internal/common",
        "//src/main/java/com/google/android/libraries/feed/hostimpl/storage",
        "//src/main/java/com/google/android/libraries/feed/testing/conformance/storage",
        "//third_party:robolectric",
        "@com_google_protobuf_javalite//:protobuf_java_lite",
        "@maven//:androidx_test_core",
        "@maven//:com_google_guava_guava",
        "@maven//:org_mockito_mockito_core",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "PersistentJournalStorageTest",
    size = "small",
    timeout = "moderate",
    srcs = ["PersistentJournalStorageTest.java"],
    manifest_values = DEFAULT_ANDROID_LOCAL_TEST_MANIFEST,
    deps = [
        "//src/main/java/com/google/android/libraries/feed/api/internal/common",
        "//src/main/java/com/google/android/libraries/feed/hostimpl/storage",
        "//src/main/java/com/google/android/libraries/feed/testing/conformance/storage",
        "//third_party:robolectric",
        "@com_google_protobuf_javalite//:protobuf_java_lite",
        "@maven//:androidx_test_core",
        "@maven//:com_google_guava_guava",
        "@maven//:com_google_truth_truth",
        "@maven//:org_mockito_mockito_core",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "PersistentJournalStorageDirectTest",
    size = "small",
    timeout = "moderate",
    srcs = ["PersistentJournalStorageDirectTest.java"],
    manifest_values = DEFAULT_ANDROID_LOCAL_TEST_MANIFEST,
    deps = [
        "//src/main/java/com/google/android/libraries/feed/api/internal/common",
        "//src/main/java/com/google/android/libraries/feed/hostimpl/storage",
        "//src/main/java/com/google/android/libraries/feed/testing/conformance/storage",
        "//third_party:robolectric",
        "@com_google_protobuf_javalite//:protobuf_java_lite",
        "@maven//:androidx_test_core",
        "@maven//:com_google_guava_guava",
        "@maven//:com_google_truth_truth",
        "@maven//:org_mockito_mockito_core",
        "@robolectric//bazel:android-all",
    ],
)
