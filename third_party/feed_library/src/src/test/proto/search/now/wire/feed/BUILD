package(
    default_testonly = 1,
    default_visibility = [
        "//src/main/java/com/google/android/libraries/feed/feedstore:__subpackages__",
        "//src/test/java/com/google/android/libraries/feed:__subpackages__",
    ],
)

licenses(["notice"])  # Apache 2

exports_files(["LICENSE"])

proto_library(
    name = "feed_test_proto",
    srcs = glob(["*.proto"]),
    deps = [
        "//src/main/proto/search/now/wire/feed:feed_proto",
    ],
)

java_proto_library(
    name = "feed_test_java_proto",
    deps = [":feed_test_proto"],
)

java_lite_proto_library(
    name = "feed_test_java_proto_lite",
    deps = [":feed_test_proto"],
)
