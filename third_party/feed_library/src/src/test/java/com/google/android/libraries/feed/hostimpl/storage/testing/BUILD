load("//src/main/java/com/google/android/libraries/feed/buildtools/testing:android_local_test.bzl", "DEFAULT_ANDROID_LOCAL_TEST_MANIFEST")

licenses(["notice"])  # Apache 2

android_local_test(
    name = "InMemoryJournalStorageTest",
    size = "small",
    timeout = "moderate",
    srcs = ["InMemoryJournalStorageTest.java"],
    manifest_values = DEFAULT_ANDROID_LOCAL_TEST_MANIFEST,
    deps = [
        "//src/main/java/com/google/android/libraries/feed/feedstore/testing",
        "//src/main/java/com/google/android/libraries/feed/hostimpl/storage/testing",
        "//src/main/java/com/google/android/libraries/feed/testing/conformance/storage",
        "//third_party:robolectric",
        "@com_google_protobuf_javalite//:protobuf_java_lite",
        "@maven//:org_mockito_mockito_core",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "InMemoryJournalStorageDirectTest",
    size = "small",
    timeout = "moderate",
    srcs = ["InMemoryJournalStorageDirectTest.java"],
    manifest_values = DEFAULT_ANDROID_LOCAL_TEST_MANIFEST,
    deps = [
        "//src/main/java/com/google/android/libraries/feed/hostimpl/storage/testing",
        "//src/main/java/com/google/android/libraries/feed/testing/conformance/storage",
        "//third_party:robolectric",
        "@com_google_protobuf_javalite//:protobuf_java_lite",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "InMemoryContentStorageTest",
    size = "small",
    timeout = "moderate",
    srcs = ["InMemoryContentStorageTest.java"],
    manifest_values = DEFAULT_ANDROID_LOCAL_TEST_MANIFEST,
    deps = [
        "//src/main/java/com/google/android/libraries/feed/feedstore/testing",
        "//src/main/java/com/google/android/libraries/feed/hostimpl/storage/testing",
        "//src/main/java/com/google/android/libraries/feed/testing/conformance/storage",
        "//third_party:robolectric",
        "@com_google_protobuf_javalite//:protobuf_java_lite",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "InMemoryContentStorageDirectTest",
    size = "small",
    timeout = "moderate",
    srcs = ["InMemoryContentStorageDirectTest.java"],
    manifest_values = DEFAULT_ANDROID_LOCAL_TEST_MANIFEST,
    deps = [
        "//src/main/java/com/google/android/libraries/feed/hostimpl/storage/testing",
        "//src/main/java/com/google/android/libraries/feed/testing/conformance/storage",
        "//third_party:robolectric",
        "@com_google_protobuf_javalite//:protobuf_java_lite",
        "@robolectric//bazel:android-all",
    ],
)
