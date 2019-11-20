package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2

android_library(
    name = "mocknetworkclient",
    srcs = glob(["*.java"]),
    deps = [
        "//src/main/java/com/google/android/libraries/feed/api/host/network",
        "//src/main/java/com/google/android/libraries/feed/common/functional",
        "//src/main/java/com/google/android/libraries/feed/common/logging",
        "//src/main/java/com/google/android/libraries/feed/feedrequestmanager",
        "//src/main/proto/search/now/wire/feed:feed_java_proto_lite",
        "//src/main/proto/search/now/wire/feed/mockserver:mock_server_java_proto_lite",
        "@com_google_protobuf_javalite//:protobuf_java_lite",
    ],
)
